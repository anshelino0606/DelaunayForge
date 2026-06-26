#include "mesh_element_info_window.h"

#include "geom/planar_mesh/planar_mesh_component.h"
#include "math/pde/pde_component.h"
#include "math/p1_element_2d.h"
#include "math/fractional_integral_operator.h"
#include "geom/delaunay/delaunay_types.h"
#include "geom/geom2d/vec.h"

#include <algorithm>

namespace fem {

std::uint64_t MeshElementInfoWindow::edge_key_(int a, int b) noexcept {
    const std::uint32_t lo = (std::uint32_t)std::min(a, b);
    const std::uint32_t hi = (std::uint32_t)std::max(a, b);
    return (std::uint64_t(lo) << 32) | std::uint64_t(hi);
}

void MeshElementInfoWindow::ensure_cache_(const PlanarMeshComponent& mesh) {
    const auto& R = mesh.triangulation_result();
    const std::size_t pc = R.points.size();
    const std::size_t tc = R.triangles.size();

    if (cached_mesh_ == &mesh && cached_point_count_ == pc && cached_tri_count_ == tc) return;

    cached_mesh_ = &mesh;
    cached_point_count_ = pc;
    cached_tri_count_   = tc;

    cached_fem_ = mesh.build_fem_mesh();
    rebuild_bc_maps_();

    const int N = cached_fem_.dof_count();
    nodal_mass_.assign((std::size_t)N, 0.0);
    for (const auto& E : cached_fem_.elems) {
        const double share = E.area / 3.0;
        nodal_mass_[(std::size_t)E.v[0]] += share;
        nodal_mass_[(std::size_t)E.v[1]] += share;
        nodal_mass_[(std::size_t)E.v[2]] += share;
    }
}

void MeshElementInfoWindow::rebuild_bc_maps_() {
    edge_bc_.clear();

    const int N = cached_fem_.dof_count();
    is_dir_.assign((std::size_t)N, 0);
    dir_val_.assign((std::size_t)N, 0.0);

    for (const auto& e : cached_fem_.edges_bc) {
        if (e.a < 0 || e.b < 0) continue;
        edge_bc_.emplace(edge_key_(e.a, e.b), e);

        if (e.type == fem::BCType::Dirichlet) {
            if (e.a >= 0 && e.a < N) { is_dir_[(std::size_t)e.a] = 1; dir_val_[(std::size_t)e.a] = e.uD; }
            if (e.b >= 0 && e.b < N) { is_dir_[(std::size_t)e.b] = 1; dir_val_[(std::size_t)e.b] = e.uD; }
        }
    }
}

void MeshElementInfoWindow::draw(const DrawInfo& info) {
    if (!visible) return;

    ImGui::Begin("Mesh Element Info", &visible);

    if (!info.mesh) {
        ImGui::TextDisabled("No mesh selected.");
        ImGui::End();
        return;
    }

    ensure_cache_(*info.mesh);

    FEMProblem prob;
    prob.mesh = &cached_fem_;

    prob.a.set_constant(1.0);
    prob.c.set_constant(0.0);
    prob.f.set_constant(0.0);
    prob.set_operator_spec(LocalEllipticSpec{});

    if (info.pde) info.pde->fill_fem_problem(prob);

    const auto& R = info.mesh->triangulation_result();

    if (!info.sel.valid()) {
        ImGui::TextDisabled("Right-click a vertex/edge/triangle in the canvas.");
        ImGui::End();
        return;
    }

    switch (info.sel.kind) {
        case CanvasInspector::Kind::Vertex:   draw_vertex_(R, info.sel.id); break;
        case CanvasInspector::Kind::Edge:     draw_edge_(R, info.sel.id); break;
        case CanvasInspector::Kind::Triangle: draw_triangle_(R, info.sel.id, prob); break;
        default: ImGui::TextDisabled("No selection."); break;
    }

    ImGui::End();
}


void MeshElementInfoWindow::draw_vertex_(const DelaunayTriangulationResult& R, int vid) {
    if (vid < 0 || (size_t)vid >= R.points.size()) return;
    const auto& P = R.points[vid];

    ImGui::Text("Vertex: %d", vid);
    ImGui::Text("Pos: (%.9f, %.9f)", (double)P.x(), (double)P.y());
    ImGui::Text("on_boundary: %s", P.on_boundary ? "true" : "false");

    ImGui::Separator();
    ImGui::Text("Incident triangles:");

    if (!R.vert2tri.empty() && (size_t)vid < R.vert2tri.size()) {
        if (R.vert2tri[vid].empty()) ImGui::TextDisabled("none");
        else for (int tid : R.vert2tri[vid]) ImGui::Text("%d", tid);
    } else {
        ImGui::TextDisabled("vert2tri not available");
    }
}

void MeshElementInfoWindow::draw_edge_(const DelaunayTriangulationResult& R, int eid) {
    if (eid < 0 || (size_t)eid >= R.edges.size()) return;

    const auto& E = R.edges[eid];
    if ((size_t)E.a >= R.points.size() || (size_t)E.b >= R.points.size()) return;

    const Point2D& A = R.points[E.a];
    const Point2D& B = R.points[E.b];

    const double L = geom2d::vec::hypot(A, B);

    ImGui::Text("Edge: %d", eid);
    ImGui::Text("Endpoints: (%d, %d)", E.a, E.b);
    ImGui::Text("Length: %.9f", L);
    ImGui::Text("on_boundary: %s", E.on_boundary ? "true" : "false");

    if (!edge_bc_.empty()) {
        const auto it = edge_bc_.find(edge_key_(E.a, E.b));
        if (it != edge_bc_.end()) {
            const auto& bc = it->second;
            ImGui::Separator();
            ImGui::Text("BC tag: %d", (int)bc.type);
            if (bc.type == fem::BCType::Dirichlet) ImGui::Text("uD = %.6g", bc.uD);
            if (bc.type == fem::BCType::Neumann)   ImGui::Text("gN = %.6g", bc.gN);
            if (bc.type == fem::BCType::Robin)     ImGui::Text("k = %.6g, g = %.6g", bc.k, bc.g);
        }
    }
}

void MeshElementInfoWindow::draw_triangle_(const DelaunayTriangulationResult& R,
                                          int tid,
                                          const FEMProblem& prob)
{
    if (tid < 0 || (size_t)tid >= R.triangles.size()) return;
    const auto& T = R.triangles[tid];
    if (!T.valid) { ImGui::Text("Triangle: %d (invalid)", tid); return; }

    const int v0 = T.v[0], v1 = T.v[1], v2 = T.v[2];
    if ((size_t)v0 >= R.points.size() || (size_t)v1 >= R.points.size() || (size_t)v2 >= R.points.size()) return;

    ImGui::Text("Triangle: %d", tid);
    ImGui::Text("Verts: (%d, %d, %d)", v0, v1, v2);

    const Point2D& p0 = R.points[v0];
    const Point2D& p1 = R.points[v1];
    const Point2D& p2 = R.points[v2];

    P1Element2D p1_element(p0, p1, p2, prob);

    ImGui::Text("Area: %.12f", p1_element.area());
    if (!p1_element.is_area_valid()) { 
        ImGui::TextDisabled("Degenerate triangle."); 
        return; 
    }

    const glm::dvec2& centroid = p1_element.centroid();

    ImGui::Text("Centroid: (%.6g, %.6g)", centroid.x, centroid.y);
    ImGui::Text("Coeffs at centroid: a=%.6g  c=%.6g  f=%.6g", 
        p1_element.a_coeff(), p1_element.c_coeff(), p1_element.f_coeff());

    p1_element.apply_edge_bc(find_edge_bc(v0, v1), 0, 1);
    p1_element.apply_edge_bc(find_edge_bc(v1, v2), 1, 2);
    p1_element.apply_edge_bc(find_edge_bc(v2, v0), 2, 0);

    const glm::dmat3x3& Ae = p1_element.Ae();
    const glm::dvec3& be = p1_element.be();

    ImGui::Separator();
    ImGui::TextUnformatted("Local (P1) Ae = Ke(a) + C(c) + Robin:");
    for (int i=0;i<3;++i) {
        ImGui::Text("[% .3e  % .3e  % .3e]", Ae[0][i], Ae[1][i], Ae[2][i]);
    }

    ImGui::TextUnformatted("Local (P1) be = vol(f) + Neumann/Robin:");
    ImGui::Text("[% .3e  % .3e  % .3e]", be.x, be.y, be.z);

    const OperatorSpec& op = prob.operator_spec();
    if (!std::holds_alternative<LocalEllipticSpec>(op)) {
        ImGui::Separator();
        std::visit([&](const auto& spec) {
            using T = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<T, FractionalIntegralSpec>) {
                ImGui::Text("Fractional enabled: Integral  s=%.6g  scale=%.6g",
                            (double)spec.s, (double)spec.scale);
            } else if constexpr (std::is_same_v<T, FractionalRegionalSpec>) {
                ImGui::Text("Fractional enabled: Regional  s=%.6g  scale=%.6g",
                            (double)spec.s, (double)spec.scale);
            } else if constexpr (std::is_same_v<T, FractionalSpectralSpec>) {
                ImGui::Text("Fractional enabled: Spectral  s=%.6g  scale=%.6g",
                            (double)spec.s, (double)spec.scale);
            }
        }, op);

        if (const auto* integral = std::get_if<FractionalIntegralSpec>(&op)) {
            FractionalElementContribution frac_element({v0, v1, v2});
            frac_element.compute(cached_fem_, *integral, prob, nodal_mass_);

            const glm::dmat3& Af = frac_element.Af();
            const glm::dvec3& bf = frac_element.bf();

            ImGui::TextUnformatted("Fractional (Integral) dense block Af on triangle vertices:");
            for (int i=0; i < 3; ++i) { 
                ImGui::Text("[% .3e  % .3e  % .3e]", Af[0][i], Af[1][i], Af[2][i]);
            }

            ImGui::TextUnformatted("Fractional RHS nodal entries (b_i = f(x_i)*m_i) for these 3 verts:");
            ImGui::Text("[% .3e  % .3e  % .3e]", bf[0], bf[1], bf[2]);

            ImGui::TextDisabled("Note: integral operator adds an exterior-interaction diagonal; there is no true per-element Ke.");
        } else if (const auto* regional = std::get_if<FractionalRegionalSpec>(&op)) {
            FractionalElementContribution frac_element({v0, v1, v2});
            frac_element.compute(cached_fem_, *regional, prob, nodal_mass_);

            const glm::dmat3& Af = frac_element.Af();
            const glm::dvec3& bf = frac_element.bf();

            ImGui::TextUnformatted("Fractional (Regional) in-domain block Af on triangle vertices:");
            for (int i=0; i < 3; ++i) {
                ImGui::Text("[% .3e  % .3e  % .3e]", Af[0][i], Af[1][i], Af[2][i]);
            }

            ImGui::TextUnformatted("Fractional RHS nodal entries (b_i = f(x_i)*m_i) for these 3 verts:");
            ImGui::Text("[% .3e  % .3e  % .3e]", bf[0], bf[1], bf[2]);

            ImGui::TextDisabled("Note: regional operator only includes in-domain interactions.");
        } else if (std::holds_alternative<FractionalSpectralSpec>(op)) {
            ImGui::TextDisabled("Spectral: local K/M/C are meaningful; operator acts via eigenmodes (see spectral solver path).");
        } else {
            ImGui::TextDisabled("Regional/other: not shown here (define what matrix you want to inspect).");
        }
    }

    if (p1_element.apply_dirichlet_elimination(is_dir_, dir_val_)) {
        const glm::ivec3& dirichlet_vertices = p1_element.dirichlet_vertices();
        const glm::dmat3& Ae_eff = p1_element.Ae_eff();
        const glm::dvec3& be_eff = p1_element.be_eff();

        ImGui::Separator();
        ImGui::Text("Dirichlet on vertices: (%s, %s, %s)",
            dirichlet_vertices[0] ? "D" : "free", dirichlet_vertices[1] ? "D" : "free", dirichlet_vertices[2] ? "D" : "free");

        ImGui::TextUnformatted("Effective local contribution (after elimination view):");
        for (int i=0;i<3;++i) {
             ImGui::Text("[% .3e  % .3e  % .3e]", Ae_eff[0][i], Ae_eff[1][i], Ae_eff[2][i]);
        }

        ImGui::Text("be_eff: [% .3e  % .3e  % .3e]", be_eff[0], be_eff[1], be_eff[2]);
    }
}

const FEMMesh::EdgeBC* MeshElementInfoWindow::find_edge_bc(int v1, int v2) const {
    const auto it = edge_bc_.find(edge_key_(v1, v2));
    if (it == edge_bc_.end()) return nullptr;

    return &it->second;
}

} // namespace fem
