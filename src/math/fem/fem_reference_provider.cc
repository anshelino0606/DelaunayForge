#include "math/fem/field/fem_reference_provider.h"
#include "math/boundary_condition.h"
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
        M.id = to_index(out.nodes.size());

        const int mid_id = static_cast<int>(M.id);
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

        FEMMesh::Elem t0{{to_index_or_invalid(v0), to_index_or_invalid(m01), to_index_or_invalid(m20)}, 0.0};
        FEMMesh::Elem t1{{to_index_or_invalid(m01), to_index_or_invalid(v1), to_index_or_invalid(m12)}, 0.0};
        FEMMesh::Elem t2{{to_index_or_invalid(m20), to_index_or_invalid(m12), to_index_or_invalid(v2)}, 0.0};
        FEMMesh::Elem t3{{to_index_or_invalid(m01), to_index_or_invalid(m12), to_index_or_invalid(m20)}, 0.0};

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
        e.a = to_index_or_invalid(std::min(a, b));
        e.b = to_index_or_invalid(std::max(a, b));
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

static DelaunayTriangulationResult triangulation_with_live_bcs_(
    const PlanarMeshComponent* mesh
) {
    if (!mesh) return {};

    DelaunayTriangulationResult result = mesh->triangulation_result();
    for (const BoundaryCondition* bc : mesh->boundary_conditions()) {
        if (!bc || bc->type() == BoundaryConditionType::None) continue;
        bc->apply(result);
    }
    return result;
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

    FEMMesh refined_mesh;
    if (req.refinement_strategy == ReferenceRefinementStrategy::UniformTriangulationSubdivision) {
        DelaunayTriangulationResult refined_tri = triangulation_with_live_bcs_(mesh_);
        if (refined_tri.points.empty() || refined_tri.triangles.empty()) {
            LOGT_ERROR(LogMath, "FEMReferenceProvider: base triangulation is empty.");
            out.error_message = "Base triangulation is empty";
            return false;
        }

        for (int k = 0; k < req.refinement_level; ++k) {
            refined_tri = refine_delaunay_uniform(refined_tri);
        }

        refined_mesh = build_fem_mesh(refined_tri);
        out.triangulation = std::move(refined_tri);
        out.has_triangulation = true;
    } else {
        refined_mesh = mesh_->build_fem_mesh();
        if (refined_mesh.nodes.empty() || refined_mesh.elems.empty()) {
            LOGT_ERROR(LogMath, "FEMReferenceProvider: base FEM mesh is empty.");
            out.error_message = "Base FEM mesh is empty";
            return false;
        }
    }

    if (refined_mesh.nodes.empty() || refined_mesh.elems.empty()) {
        LOGT_ERROR(LogMath, "FEMReferenceProvider: refined FEM mesh is empty.");
        out.error_message = "Refined FEM mesh is empty";
        return false;
    }

    // Guard against runaway uniform refinement (node/element counts grow ~4x per level).
    // For triangulation-based refinement the mesh has already been refined above, so we cap
    // the resulting mesh directly and only apply the growth estimate to FEM subdivision.
    const std::size_t current_dofs = refined_mesh.nodes.size();
    constexpr std::size_t kMaxRefDofs = 200000; // safety cap to avoid OOM/timeouts
    if (current_dofs > kMaxRefDofs) {
        out.error_message =
            "Reference mesh exceeds safety DOF cap before solve. Reduce levels or use an exact solution / different reference strategy.";
        LOGT_ERROR(LogMath,
                   "FEMReferenceProvider: %s dofs=%zu ref_level=%d strategy=%s",
                   out.error_message.c_str(),
                   current_dofs,
                   req.refinement_level,
                   req.refinement_strategy == ReferenceRefinementStrategy::UniformTriangulationSubdivision
                       ? "triangulation"
                       : "fem");
        return false;
    }
    if (req.refinement_strategy == ReferenceRefinementStrategy::UniformFemSubdivision) {
        std::size_t est = current_dofs;
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
            LOGT_ERROR(LogMath,
                       "FEMReferenceProvider: %s base_dofs=%zu ref_level=%d",
                       out.error_message.c_str(),
                       current_dofs,
                       req.refinement_level);
            return false;
        }
    }

    if (req.refinement_strategy == ReferenceRefinementStrategy::UniformFemSubdivision) {
        for (int k = 0; k < req.refinement_level; ++k) {
            refined_mesh = refine_fem_mesh_uniform_(refined_mesh);
            if (refined_mesh.nodes.size() > kMaxRefDofs) {
                out.error_message = "Reference mesh exceeded safety DOF cap during refinement.";
                LOGT_ERROR(LogMath, "FEMReferenceProvider: %s dofs=%zu ref_level=%d", out.error_message.c_str(), refined_mesh.nodes.size(), req.refinement_level);
                return false;
            }
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

    LOGT_INFO(LogMath,
              "FEMReferenceProvider: built reference with refinement level %d (strategy=%s)",
              req.refinement_level,
              req.refinement_strategy == ReferenceRefinementStrategy::UniformTriangulationSubdivision
                  ? "triangulation"
                  : "fem");
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

    FEMMesh refined_mesh;
    if (req.refinement_strategy == ReferenceRefinementStrategy::UniformTriangulationSubdivision) {
        DelaunayTriangulationResult refined_tri = triangulation_with_live_bcs_(mesh_);
        if (refined_tri.points.empty() || refined_tri.triangles.empty()) {
            LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: base triangulation is empty.");
            out.error_message = "Base triangulation is empty";
            return false;
        }

        for (int k = 0; k < req.refinement_level; ++k) {
            refined_tri = refine_delaunay_uniform(refined_tri);
        }

        refined_mesh = build_fem_mesh_all_boundary_dirichlet(refined_tri, u_exact_);
        out.triangulation = std::move(refined_tri);
        out.has_triangulation = true;
    } else {
        refined_mesh = build_base_mesh_with_exact_dirichlet_(mesh_, u_exact_);
        if (refined_mesh.nodes.empty() || refined_mesh.elems.empty()) {
            LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: base FEM mesh is empty.");
            out.error_message = "Base FEM mesh is empty";
            return false;
        }
    }

    if (refined_mesh.nodes.empty() || refined_mesh.elems.empty()) {
        LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: refined FEM mesh is empty.");
        out.error_message = "Refined FEM mesh is empty";
        return false;
    }

    // Same refinement guard as FEMReferenceProvider.
    const std::size_t current_dofs = refined_mesh.nodes.size();
    constexpr std::size_t kMaxRefDofs = 200000;
    if (current_dofs > kMaxRefDofs) {
        out.error_message =
            "Reference mesh exceeds safety DOF cap before solve. Reduce levels or use a different study strategy.";
        LOGT_ERROR(LogMath,
                   "FEMReferenceProviderExactDirichlet: %s dofs=%zu ref_level=%d strategy=%s",
                   out.error_message.c_str(),
                   current_dofs,
                   req.refinement_level,
                   req.refinement_strategy == ReferenceRefinementStrategy::UniformTriangulationSubdivision
                       ? "triangulation"
                       : "fem");
        return false;
    }
    if (req.refinement_strategy == ReferenceRefinementStrategy::UniformFemSubdivision) {
        std::size_t est = current_dofs;
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
                       out.error_message.c_str(), current_dofs, req.refinement_level);
            return false;
        }
    }

    if (req.refinement_strategy == ReferenceRefinementStrategy::UniformFemSubdivision) {
        for (int k = 0; k < req.refinement_level; ++k) {
            refined_mesh = refine_fem_mesh_uniform_(refined_mesh);
            if (refined_mesh.nodes.size() > kMaxRefDofs) {
                out.error_message = "Reference mesh exceeded safety DOF cap during refinement.";
                LOGT_ERROR(LogMath, "FEMReferenceProviderExactDirichlet: %s dofs=%zu ref_level=%d",
                           out.error_message.c_str(), refined_mesh.nodes.size(), req.refinement_level);
                return false;
            }
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
    LOGT_INFO(LogMath,
              "FEMReferenceProviderExactDirichlet: built reference with refinement level %d (strategy=%s)",
              req.refinement_level,
              req.refinement_strategy == ReferenceRefinementStrategy::UniformTriangulationSubdivision
                  ? "triangulation"
                  : "fem");
    return true;
}

DelaunayTriangulationResult refine_delaunay_uniform(
    const DelaunayTriangulationResult& input)
{
    if (input.triangles.empty() || input.points.empty()) {
        return input;
    }

    DelaunayTriangulationResult refined;
    refined.points = input.points;
    refined.points.reserve(input.points.size() + input.triangles.size() * 3);

    std::unordered_map<std::uint64_t, int> edge_to_midpoint_idx;
    edge_to_midpoint_idx.reserve(input.triangles.size() * 3);

    std::unordered_map<std::uint64_t, EdgeInfo> input_boundary_info;
    input_boundary_info.reserve(input.edges.size() + input.boundary_edges.size());
    std::unordered_map<std::uint64_t, EdgeInfo> refined_boundary_info;
    refined_boundary_info.reserve(input.edges.size() * 2 + input.boundary_edges.size() * 2);
    for (const auto& edge_info : input.edges) {
        if (!edge_info.on_boundary) continue;
        input_boundary_info.emplace(pack_edge_key(edge_info.a, edge_info.b), edge_info);
    }
    for (const auto& edge : input.boundary_edges) {
        const std::uint64_t key = pack_edge_key(edge.a, edge.b);
        if (input_boundary_info.find(key) == input_boundary_info.end()) {
            EdgeInfo edge_info{};
            edge_info.a = std::min(edge.a, edge.b);
            edge_info.b = std::max(edge.a, edge.b);
            edge_info.on_boundary = true;
            input_boundary_info.emplace(key, edge_info);
        }
    }

    auto midpoint_index = [&](int a, int b) -> int {
        const std::uint64_t key = pack_edge_key(a, b);
        auto it = edge_to_midpoint_idx.find(key);
        if (it != edge_to_midpoint_idx.end()) {
            return it->second;
        }

        if (a < 0 || b < 0 || static_cast<std::size_t>(a) >= input.points.size() ||
            static_cast<std::size_t>(b) >= input.points.size()) {
            return -1;
        }

        const Point2D& p0 = input.points[static_cast<std::size_t>(a)];
        const Point2D& p1 = input.points[static_cast<std::size_t>(b)];

        Point2D midpoint;
        midpoint.p.x = 0.5 * (p0.x() + p1.x());
        midpoint.p.y = 0.5 * (p0.y() + p1.y());
        midpoint.id = static_cast<int>(refined.points.size());

        const auto bit = input_boundary_info.find(key);
        midpoint.on_boundary = (bit != input_boundary_info.end());

        const int idx = midpoint.id;
        refined.points.push_back(midpoint);
        edge_to_midpoint_idx.emplace(key, idx);

        if (bit != input_boundary_info.end()) {
            EdgeInfo child0 = bit->second;
            child0.a = std::min(a, idx);
            child0.b = std::max(a, idx);
            child0.on_boundary = true;
            child0.tri_left = -1;
            child0.tri_right = -1;
            refined_boundary_info[pack_edge_key(child0.a, child0.b)] = child0;

            EdgeInfo child1 = bit->second;
            child1.a = std::min(idx, b);
            child1.b = std::max(idx, b);
            child1.on_boundary = true;
            child1.tri_left = -1;
            child1.tri_right = -1;
            refined_boundary_info[pack_edge_key(child1.a, child1.b)] = child1;
        }

        return idx;
    };

    refined.triangles.reserve(4 * input.triangles.size());
    refined.tri2vert.reserve(4 * input.triangles.size());

    auto append_tri = [&](int a, int b, int c) {
        Tri child(a, b, c, static_cast<int>(refined.triangles.size()));
        child.valid = true;
        refined.triangles.push_back(child);
        refined.tri2vert.push_back(child.v);
    };

    for (const auto& tri : input.triangles) {
        if (!tri.valid) continue;

        const int v0 = tri.v.x;
        const int v1 = tri.v.y;
        const int v2 = tri.v.z;

        const int m01 = midpoint_index(v0, v1);
        const int m12 = midpoint_index(v1, v2);
        const int m20 = midpoint_index(v2, v0);

        if (m01 < 0 || m12 < 0 || m20 < 0) {
            LOGT_ERROR(LogMath, "refine_delaunay_uniform: failed to create midpoint for triangle %d", tri.id);
            continue;
        }

        append_tri(v0, m01, m20);
        append_tri(m01, v1, m12);
        append_tri(m20, m12, v2);
        append_tri(m01, m12, m20);
    }

    struct AccEdge {
        int a = -1;
        int b = -1;
        int tri_left = -1;
        int tri_right = -1;
    };

    refined.tri2edge.assign(refined.triangles.size(), {-1, -1, -1});
    refined.tri_neighbors.assign(refined.triangles.size(), {-1, -1, -1});
    refined.vert2tri.assign(refined.points.size(), {});

    std::unordered_map<std::uint64_t, int> edge_index;
    edge_index.reserve(refined.triangles.size() * 3);
    std::vector<AccEdge> acc_edges;
    acc_edges.reserve(refined.triangles.size() * 3);

    auto add_edge = [&](int a, int b, int tri_idx) -> int {
        const std::uint64_t key = pack_edge_key(a, b);
        auto [it, inserted] = edge_index.emplace(key, static_cast<int>(acc_edges.size()));
        if (inserted) {
            AccEdge edge;
            edge.a = std::min(a, b);
            edge.b = std::max(a, b);
            edge.tri_left = tri_idx;
            acc_edges.push_back(edge);
            return it->second;
        }

        AccEdge& edge = acc_edges[static_cast<std::size_t>(it->second)];
        if (edge.tri_left == -1) edge.tri_left = tri_idx;
        else edge.tri_right = tri_idx;
        return it->second;
    };

    for (std::size_t i = 0; i < refined.triangles.size(); ++i) {
        const Tri& tri = refined.triangles[i];
        const int e0 = add_edge(tri.v.x, tri.v.y, static_cast<int>(i));
        const int e1 = add_edge(tri.v.y, tri.v.z, static_cast<int>(i));
        const int e2 = add_edge(tri.v.z, tri.v.x, static_cast<int>(i));
        refined.tri2edge[i] = {e0, e1, e2};

        refined.vert2tri[static_cast<std::size_t>(tri.v.x)].push_back(static_cast<int>(i));
        refined.vert2tri[static_cast<std::size_t>(tri.v.y)].push_back(static_cast<int>(i));
        refined.vert2tri[static_cast<std::size_t>(tri.v.z)].push_back(static_cast<int>(i));
    }

    refined.edges.reserve(acc_edges.size());
    for (const auto& acc_edge : acc_edges) {
        EdgeInfo edge_info{};
        edge_info.a = acc_edge.a;
        edge_info.b = acc_edge.b;
        edge_info.tri_left = acc_edge.tri_left;
        edge_info.tri_right = acc_edge.tri_right;
        edge_info.on_boundary = (acc_edge.tri_right == -1);

        const auto child_bit = refined_boundary_info.find(pack_edge_key(acc_edge.a, acc_edge.b));
        if (child_bit != refined_boundary_info.end()) {
            edge_info.boundary_tag = child_bit->second.boundary_tag;
            edge_info.bc = child_bit->second.bc;
        } else {
            const auto bit = input_boundary_info.find(pack_edge_key(acc_edge.a, acc_edge.b));
            if (bit != input_boundary_info.end()) {
                edge_info.boundary_tag = bit->second.boundary_tag;
                edge_info.bc = bit->second.bc;
            }
        }

        refined.edges.push_back(edge_info);
        if (edge_info.on_boundary) {
            refined.boundary_edges.push_back({edge_info.a, edge_info.b});
            if (static_cast<std::size_t>(edge_info.a) < refined.points.size()) {
                refined.points[static_cast<std::size_t>(edge_info.a)].on_boundary = true;
            }
            if (static_cast<std::size_t>(edge_info.b) < refined.points.size()) {
                refined.points[static_cast<std::size_t>(edge_info.b)].on_boundary = true;
            }
        }
    }

    for (std::size_t i = 0; i < refined.triangles.size(); ++i) {
        const glm::ivec3 tri_edges = refined.tri2edge[i];
        glm::ivec3 neighbors{-1, -1, -1};
        for (int k = 0; k < 3; ++k) {
            const int edge_idx = tri_edges[k];
            if (edge_idx < 0 || static_cast<std::size_t>(edge_idx) >= refined.edges.size()) continue;
            const EdgeInfo& edge_info = refined.edges[static_cast<std::size_t>(edge_idx)];
            neighbors[k] = (edge_info.tri_left == static_cast<int>(i)) ? edge_info.tri_right : edge_info.tri_left;
        }
        refined.triangles[i].neighbors = neighbors;
        refined.tri_neighbors[i] = neighbors;
    }

    refined.point_count = static_cast<int>(refined.points.size());
    refined.triangle_count = static_cast<int>(refined.triangles.size());

    refined.min_angle = input.min_angle;
    refined.median_angle = input.median_angle;
    refined.avg_angle = input.avg_angle;

    return refined;
}

} // namespace fem