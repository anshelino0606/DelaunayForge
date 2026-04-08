#include "math/fem/field/fem_reference_provider.h"
#include "math/pde/pde_component.h"
#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace fem {

static inline std::uint64_t pack_edge_key(int a, int b) {
    const std::uint32_t lo = static_cast<std::uint32_t>(std::min(a, b));
    const std::uint32_t hi = static_cast<std::uint32_t>(std::max(a, b));
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}

static FEMMesh refine_fem_mesh_uniform_(const FEMMesh& input) {
    FEMMesh out;
    out.nodes = input.nodes;
    out.elems.reserve(input.elems.size() * 4);
    out.edges_bc.reserve(input.edges_bc.size() * 2);

    std::unordered_map<std::uint64_t, int> edge_to_mid;
    edge_to_mid.reserve(input.elems.size() * 3);

    auto midpoint_node = [&](int a, int b) -> int {
        const std::uint64_t key = pack_edge_key(a, b);
        auto it = edge_to_mid.find(key);
        if (it != edge_to_mid.end()) {
            return it->second;
        }

        if (a < 0 || b < 0 || (std::size_t)a >= out.nodes.size() || (std::size_t)b >= out.nodes.size()) {
            return -1;
        }

        const auto& A = out.nodes[(std::size_t)a];
        const auto& B = out.nodes[(std::size_t)b];

        FEMMesh::Node M;
        M.x = 0.5 * (A.x + B.x);
        M.y = 0.5 * (A.y + B.y);
        M.id = (int)out.nodes.size();

        const int mid_id = M.id;
        out.nodes.push_back(M);
        edge_to_mid.emplace(key, mid_id);
        return mid_id;
    };

    auto tri_area = [&](int i0, int i1, int i2) -> double {
        const auto& A = out.nodes[(std::size_t)i0];
        const auto& B = out.nodes[(std::size_t)i1];
        const auto& C = out.nodes[(std::size_t)i2];
        const double cross = (B.x - A.x) * (C.y - A.y) - (C.x - A.x) * (B.y - A.y);
        return 0.5 * std::abs(cross);
    };

    for (const auto& e : input.elems) {
        const int v0 = e.v[0];
        const int v1 = e.v[1];
        const int v2 = e.v[2];

        const int m01 = midpoint_node(v0, v1);
        const int m12 = midpoint_node(v1, v2);
        const int m20 = midpoint_node(v2, v0);

        if (m01 < 0 || m12 < 0 || m20 < 0) {
            continue;
        }

        FEMMesh::Elem t0{{v0, m01, m20}, 0.0};
        FEMMesh::Elem t1{{m01, v1, m12}, 0.0};
        FEMMesh::Elem t2{{m20, m12, v2}, 0.0};
        FEMMesh::Elem t3{{m01, m12, m20}, 0.0};

        t0.area = tri_area(t0.v[0], t0.v[1], t0.v[2]);
        t1.area = tri_area(t1.v[0], t1.v[1], t1.v[2]);
        t2.area = tri_area(t2.v[0], t2.v[1], t2.v[2]);
        t3.area = tri_area(t3.v[0], t3.v[1], t3.v[2]);

        out.elems.push_back(t0);
        out.elems.push_back(t1);
        out.elems.push_back(t2);
        out.elems.push_back(t3);
    }

    auto emit_bc_edge = [&](int a, int b, const FEMMesh::EdgeBC& src) {
        FEMMesh::EdgeBC e = src;
        e.a = std::min(a, b);
        e.b = std::max(a, b);
        out.edges_bc.push_back(e);
    };

    for (const auto& bc : input.edges_bc) {
        const int m = midpoint_node(bc.a, bc.b);
        if (m < 0) continue;
        emit_bc_edge(bc.a, m, bc);
        emit_bc_edge(m, bc.b, bc);
    }

    return out;
}

static FEMMesh build_base_mesh_with_exact_dirichlet_(
    const PlanarMeshComponent* mesh,
    const std::function<double(double, double)>& u_exact
) {
    if (!mesh) return {};
    const auto& R = mesh->triangulation_result();
    return ::fem::build_fem_mesh_all_boundary_dirichlet(R, u_exact);
}

bool FEMReferenceProvider::solve_reference(
    const ReferenceSolveRequest& req, 
    ReferenceSolution& out) const 
{
    out = ReferenceSolution{};

    if (!pde_ || !mesh_) {
        LOGT_ERROR(LogMath, "FEMReferenceProvider: invalid PDE or mesh.");
        out.error_message = "Invalid PDE or mesh";
        return false;
    }

    FEMMesh refined_mesh = mesh_->build_fem_mesh();
    if (refined_mesh.nodes.empty() || refined_mesh.elems.empty()) {
        LOGT_ERROR(LogMath, "FEMReferenceProvider: base FEM mesh is empty.");
        out.error_message = "Base FEM mesh is empty";
        return false;
    }

    // Guard against runaway uniform refinement (node/element counts grow ~4x per level).
    // This is especially important for already-dense meshes (e.g. Sierpinski level-4).
    const std::size_t base_dofs = refined_mesh.nodes.size();
    constexpr std::size_t kMaxRefDofs = 200000; // safety cap to avoid OOM/timeouts
    {
        std::size_t est = base_dofs;
        for (int k = 0; k < req.refinement_level; ++k) {
            if (est > kMaxRefDofs / 4) {
                est = kMaxRefDofs + 1;
                break;
            }
            est *= 4;
        }
        if (est > kMaxRefDofs) {
            out.error_message =
                "Requested refinement level would create an excessively large reference mesh (uniform refinement). "
                "Reduce levels or use an exact solution / different reference strategy.";
            LOGT_ERROR(LogMath, "FEMReferenceProvider: %s base_dofs=%zu ref_level=%d", out.error_message.c_str(), base_dofs, req.refinement_level);
            return false;
        }
    }

    for (int k = 0; k < req.refinement_level; ++k) {
        refined_mesh = refine_fem_mesh_uniform_(refined_mesh);
        if (refined_mesh.nodes.size() > kMaxRefDofs) {
            out.error_message = "Reference mesh exceeded safety DOF cap during refinement.";
            LOGT_ERROR(LogMath, "FEMReferenceProvider: %s dofs=%zu ref_level=%d", out.error_message.c_str(), refined_mesh.nodes.size(), req.refinement_level);
            return false;
        }
    }

    out.mesh = std::move(refined_mesh);

    FEMProblem prob;
    pde_->fill_fem_problem(prob);
    prob.mesh = &out.mesh;

    DifferentialEquationSolution ref_sol;
    FEMSystem sys = assemble_and_solve_auto_P1(prob, ref_sol);

    if (!ref_sol.is_ready()) {
        out.error_message = "Assembly/solve failed for this reference mesh.";
        LOGT_ERROR(LogMath, "FEMReferenceProvider: %s (ref_level=%d, dofs=%zu)", out.error_message.c_str(), req.refinement_level, out.mesh.nodes.size());
        return false;
    }

    out.sol = std::move(ref_sol);
    out.sys = std::move(sys);
    out.has_sys = true;

    LOGT_INFO(LogMath, "FEMReferenceProvider: built reference with refinement level %d", req.refinement_level);
    return true;
}

bool FEMReferenceProviderExactDirichlet::solve_reference(
    const ReferenceSolveRequest& req,
    ReferenceSolution& out) const
{
    out = ReferenceSolution{};

    if (!pde_ || !mesh_) {
        LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: invalid PDE or mesh.");
        out.error_message = "Invalid PDE or mesh";
        return false;
    }

    FEMMesh refined_mesh = build_base_mesh_with_exact_dirichlet_(mesh_, u_exact_);
    if (refined_mesh.nodes.empty() || refined_mesh.elems.empty()) {
        LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: base FEM mesh is empty.");
        out.error_message = "Base FEM mesh is empty";
        return false;
    }

    // Same refinement guard as FEMReferenceProvider.
    const std::size_t base_dofs = refined_mesh.nodes.size();
    constexpr std::size_t kMaxRefDofs = 200000;
    {
        std::size_t est = base_dofs;
        for (int k = 0; k < req.refinement_level; ++k) {
            if (est > kMaxRefDofs / 4) {
                est = kMaxRefDofs + 1;
                break;
            }
            est *= 4;
        }
        if (est > kMaxRefDofs) {
            out.error_message =
                "Requested refinement level would create an excessively large reference mesh (uniform refinement). "
                "Reduce levels or use a different study strategy.";
            LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: %s base_dofs=%zu ref_level=%d",
                       out.error_message.c_str(), base_dofs, req.refinement_level);
            return false;
        }
    }

    for (int k = 0; k < req.refinement_level; ++k) {
        refined_mesh = refine_fem_mesh_uniform_(refined_mesh);
        if (refined_mesh.nodes.size() > kMaxRefDofs) {
            out.error_message = "Reference mesh exceeded safety DOF cap during refinement.";
            LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: %s dofs=%zu ref_level=%d",
                       out.error_message.c_str(), refined_mesh.nodes.size(), req.refinement_level);
            return false;
        }
    }

    out.mesh = std::move(refined_mesh);

    FEMProblem prob;
    pde_->fill_fem_problem(prob);
    prob.mesh = &out.mesh;

    DifferentialEquationSolution ref_sol;
    FEMSystem sys = assemble_and_solve_auto_P1(prob, ref_sol);

    if (!ref_sol.is_ready()) {
        out.error_message = "Assembly/solve failed for this mesh.";
        LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: %s (ref_level=%d, dofs=%zu)",
                   out.error_message.c_str(), req.refinement_level, out.mesh.nodes.size());
        return false;
    }

    out.sol = std::move(ref_sol);
    out.sys = std::move(sys);
    out.has_sys = true;
    return true;
}

DelaunayTriangulationResult refine_delaunay_uniform(
    const DelaunayTriangulationResult& input)
{
    
    if (input.triangles.empty() || input.points.empty()) {
        return input;
    }

    DelaunayTriangulationResult refined;
    
    const size_t num_edges = input.edges.size();
    refined.points.reserve(input.points.size() + num_edges);
    
    refined.points = input.points;
    
    std::unordered_map<int, int> edge_to_midpoint_idx;
    
    for (size_t e_id = 0; e_id < input.edges.size(); ++e_id) {
        const EdgeInfo& edge_info = input.edges[e_id];
        Point2D p0 = input.points[edge_info.a];
        Point2D p1 = input.points[edge_info.b];
        
        Point2D midpoint;
        midpoint.p.x = 0.5 * (p0.x() + p1.x());
        midpoint.p.y = 0.5 * (p0.y() + p1.y());
        
        edge_to_midpoint_idx[static_cast<int>(e_id)] = 
            static_cast<int>(refined.points.size());
        refined.points.push_back(midpoint);
    }
    
    refined.triangles.reserve(4 * input.triangles.size());
    refined.tri2vert.reserve(4 * input.tri2vert.size());
    
    for (size_t t = 0; t < input.triangles.size(); ++t) {
        const auto& tri = input.triangles[t];
        const auto& tri_edges = input.tri2edge[t];
        
        int v0 = input.tri2vert[t].x;
        int v1 = input.tri2vert[t].y;
        int v2 = input.tri2vert[t].z;
        
        // Edge 0: (v0, v1)
        // Edge 1: (v1, v2)
        // Edge 2: (v2, v0)
        auto it01 = edge_to_midpoint_idx.find(tri_edges.x);
        auto it12 = edge_to_midpoint_idx.find(tri_edges.y);
        auto it20 = edge_to_midpoint_idx.find(tri_edges.z);
        
        if (it01 == edge_to_midpoint_idx.end() || 
            it12 == edge_to_midpoint_idx.end() || 
            it20 == edge_to_midpoint_idx.end()) {
            LOGT_ERROR(LogMath, "refine_delaunay_uniform: edge index not found in midpoint map");
            continue;
        }
        
        int m01 = it01->second;
        int m12 = it12->second;
        int m20 = it20->second;
        
        refined.triangles.push_back(tri);
        refined.tri2vert.push_back({v0, m01, m20});
        
        refined.triangles.push_back(tri);
        refined.tri2vert.push_back({m01, v1, m12});
        
        refined.triangles.push_back(tri);
        refined.tri2vert.push_back({m20, m12, v2});
        
        refined.triangles.push_back(tri);
        refined.tri2vert.push_back({m01, m12, m20});
    }
    
    for (const Edge& edge : input.boundary_edges) {
        for (size_t e_id = 0; e_id < input.edges.size(); ++e_id) {
            const EdgeInfo& edge_info = input.edges[e_id];
            if ((edge_info.a == edge.a && edge_info.b == edge.b) ||
                (edge_info.a == edge.b && edge_info.b == edge.a)) {
                
                int m_idx = edge_to_midpoint_idx[static_cast<int>(e_id)];
                
                // Two refined boundary edges
                refined.boundary_edges.push_back({edge.a, m_idx});
                refined.boundary_edges.push_back({m_idx, edge.b});
                break;
            }
        }
    }
    
    refined.point_count = static_cast<int>(refined.points.size());
    refined.triangle_count = static_cast<int>(refined.triangles.size());
    
    refined.min_angle = input.min_angle;
    refined.median_angle = input.median_angle;
    refined.avg_angle = input.avg_angle;
    
    // Note: Adjacency structures (vert2tri, tri_neighbors) are not populated here.
    // They will be recomputed by the mesh building process if needed.
    
    return refined;
}

} // namespace fem