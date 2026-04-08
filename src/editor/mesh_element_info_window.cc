#include "mesh_element_info_window.h"

#include "geom/planar_mesh/planar_mesh_component.h"
#include "math/pde/pde_component.h"
#include "geom/delaunay_types.h"

#include <cmath>
#include <algorithm>

namespace fem {

using PointT =
    std::remove_cv_t<std::remove_reference_t<
        decltype(std::declval<DelaunayTriangulationResult>().points[0])
    >>;

static inline glm::dvec2 p2(const PointT& P) {
    return glm::dvec2((double)P.x(), (double)P.y());
}

static inline double cross2(const glm::dvec2& a, const glm::dvec2& b) {
    return a.x*b.y - a.y*b.x;
}

static inline double tri_area_abs(const glm::dvec2& a,
                                 const glm::dvec2& b,
                                 const glm::dvec2& c)
{
    return 0.5 * std::abs(cross2(b - a, c - a));
}

static inline void p1_bc(const glm::dvec2& p0,
                         const glm::dvec2& p1,
                         const glm::dvec2& p2,
                         double b[3], double cc[3]) noexcept
{
    b[0] = p1.y - p2.y; cc[0] = p2.x - p1.x;
    b[1] = p2.y - p0.y; cc[1] = p0.x - p2.x;
    b[2] = p0.y - p1.y; cc[2] = p1.x - p0.x;
}

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
    prob.fractional.reset();

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

    const auto& A = R.points[E.a];
    const auto& B = R.points[E.b];

    const glm::dvec2 a = p2(A);
    const glm::dvec2 b = p2(B);
    const double L = std::hypot(a.x - b.x, a.y - b.y);

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

    const glm::dvec2 p0 = p2(R.points[v0]);
    const glm::dvec2 p1 = p2(R.points[v1]);
    const glm::dvec2 p2v = p2(R.points[v2]);

    const double A = tri_area_abs(p0, p1, p2v);
    ImGui::Text("Area: %.12f", A);
    if (A <= 1e-30) { ImGui::TextDisabled("Degenerate triangle."); return; }

    const double cx = (p0.x + p1.x + p2v.x) / 3.0;
    const double cy = (p0.y + p1.y + p2v.y) / 3.0;

    const double a_coeff = prob.a(cx, cy);
    const double c_coeff = prob.c(cx, cy);
    const double f_coeff = prob.f(cx, cy);

    ImGui::Text("Centroid: (%.6g, %.6g)", cx, cy);
    ImGui::Text("Coeffs at centroid: a=%.6g  c=%.6g  f=%.6g", a_coeff, c_coeff, f_coeff);

    double Ke[3][3]{};
    double Ce[3][3]{};
    double Ae[3][3]{};
    double be[3]{};

    double b[3], cc[3];
    p1_bc(p0, p1, p2v, b, cc);

    const double inv4A = 1.0 / (4.0 * A);
    const double mfac  = (c_coeff * A) / 12.0;

    for (int i=0;i<3;++i) {
        for (int j=0;j<3;++j) {
            Ke[i][j] = a_coeff * (b[i]*b[j] + cc[i]*cc[j]) * inv4A;
            Ce[i][j] = mfac * ((i==j) ? 2.0 : 1.0);
            Ae[i][j] = Ke[i][j] + Ce[i][j];
        }
    }

    const double lv = f_coeff * A / 3.0;
    be[0] += lv; be[1] += lv; be[2] += lv;

    auto add_robin_edge = [&](int la, int lb, double L, double k, double g) {
        const double m00 = k * L * (2.0/6.0);
        const double m01 = k * L * (1.0/6.0);
        const double m11 = k * L * (2.0/6.0);

        Ae[la][la] += m00;
        Ae[la][lb] += m01;
        Ae[lb][la] += m01;
        Ae[lb][lb] += m11;

        if (g != 0.0) {
            const double fg = g * L * 0.5;
            be[la] += fg;
            be[lb] += fg;
        }
    };

    auto add_neumann_edge = [&](int la, int lb, double L, double gN) {
        const double fg = gN * L * 0.5;
        be[la] += fg;
        be[lb] += fg;
    };

    auto handle_edge = [&](int a, int b, int la, int lb, const glm::dvec2& pa, const glm::dvec2& pb) {
        const auto it = edge_bc_.find(edge_key_(a,b));
        if (it == edge_bc_.end()) return;

        const auto& bc = it->second;
        const double L = std::hypot(pa.x - pb.x, pa.y - pb.y);

        if (bc.type == fem::BCType::Robin) {
            add_robin_edge(la, lb, L, bc.k, bc.g);
        } else if (bc.type == fem::BCType::Neumann) {
            add_neumann_edge(la, lb, L, bc.gN);
        }
    };

    handle_edge(v0, v1, 0, 1, p0, p1);
    handle_edge(v1, v2, 1, 2, p1, p2v);
    handle_edge(v2, v0, 2, 0, p2v, p0);

    ImGui::Separator();
    ImGui::TextUnformatted("Local (P1) Ae = Ke(a) + C(c) + Robin:");
    for (int i=0;i<3;++i) ImGui::Text("[% .3e  % .3e  % .3e]", Ae[i][0], Ae[i][1], Ae[i][2]);

    ImGui::TextUnformatted("Local (P1) be = vol(f) + Neumann/Robin:");
    ImGui::Text("[% .3e  % .3e  % .3e]", be[0], be[1], be[2]);

    if (prob.fractional) {
        const auto cfg = *prob.fractional;
        ImGui::Separator();
        const uint32_t frac_type = cfg.type.operator uint32_t();
        ImGui::Text("Fractional enabled: type=%u  s=%.6g  scale=%.6g",
                    (unsigned)frac_type, (double)cfg.s, (double)cfg.scale);



        if (cfg.type == fem::FractionalType::Integral) {
            double Af[3][3]{}; // init 0
            const int gv[3] = { v0, v1, v2 };

            const double s_exp = 1.0 + (double)cfg.s; // denom = (r^2)^{1+s}
            const double C_scale = (double)cfg.scale;

            auto node_xy = [&](int gi, double& x, double& y) {
                const auto& n = cached_fem_.nodes[(size_t)gi];
                x = n.x; y = n.y;
            };

            auto w_ij = [&](int i, int j) -> double {
                double xi, yi, xj, yj;
                node_xy(i, xi, yi);
                node_xy(j, xj, yj);
                const double dx = xi - xj;
                const double dy = yi - yj;
                const double r2 = dx*dx + dy*dy;
                if (r2 == 0.0) return 0.0;
                const double mi = nodal_mass_[(size_t)i];
                const double mj = nodal_mass_[(size_t)j];
                return C_scale * mi * mj / std::pow(r2, s_exp);
            };

            for (int a=0; a<3; ++a) {
                for (int b=0; b<3; ++b) {
                    if (a == b) continue;
                    const double w = w_ij(gv[a], gv[b]);
                    Af[a][b] = -w;
                }
            }

            const int N = cached_fem_.dof_count();
            for (int a=0; a<3; ++a) {
                const int i = gv[a];
                double diag = 0.0;
                for (int j=0; j<N; ++j) {
                    if (j == i) continue;
                    diag += w_ij(i, j);
                }
                Af[a][a] = diag;
            }

            ImGui::TextUnformatted("Fractional (Integral) dense block Af on triangle vertices:");
            for (int i=0;i<3;++i) ImGui::Text("[% .3e  % .3e  % .3e]", Af[i][0], Af[i][1], Af[i][2]);

            double bf[3]{};
            for (int a=0; a<3; ++a) {
                const int i = gv[a];
                const auto& n = cached_fem_.nodes[(size_t)i];
                const double fi = prob.f(n.x, n.y);
                bf[a] = fi * nodal_mass_[(size_t)i];
            }
            ImGui::TextUnformatted("Fractional RHS nodal entries (b_i = f(x_i)*m_i) for these 3 verts:");
            ImGui::Text("[% .3e  % .3e  % .3e]", bf[0], bf[1], bf[2]);

            ImGui::TextDisabled("Note: integral fractional operator is nonlocal; there is no true per-element Ke.");
        } else if (cfg.type == fem::FractionalType::Spectral) {
            ImGui::TextDisabled("Spectral: local K/M/C are meaningful; operator acts via eigenmodes (see spectral solver path).");
        } else {
            ImGui::TextDisabled("Regional/other: not shown here (define what matrix you want to inspect).");
        }
    }

    const int gv2[3] = { v0, v1, v2 };
    const bool d0 = (v0 >= 0 && v0 < (int)is_dir_.size()) ? (is_dir_[(size_t)v0] != 0) : false;
    const bool d1 = (v1 >= 0 && v1 < (int)is_dir_.size()) ? (is_dir_[(size_t)v1] != 0) : false;
    const bool d2 = (v2 >= 0 && v2 < (int)is_dir_.size()) ? (is_dir_[(size_t)v2] != 0) : false;

    if (d0 || d1 || d2) {
        double be_eff[3] = { be[0], be[1], be[2] };
        double Ae_eff[3][3];
        for (int i=0;i<3;++i) for (int j=0;j<3;++j) Ae_eff[i][j] = Ae[i][j];

        auto uD = [&](int gvid) -> double {
            if (gvid < 0 || gvid >= (int)dir_val_.size()) return 0.0;
            return dir_val_[(size_t)gvid];
        };

        for (int li=0; li<3; ++li) {
            const int gi = gv2[li];
            const bool di = (gi >= 0 && gi < (int)is_dir_.size()) ? (is_dir_[(size_t)gi] != 0) : false;
            if (di) continue;

            for (int lj=0; lj<3; ++lj) {
                const int gj = gv2[lj];
                const bool dj = (gj >= 0 && gj < (int)is_dir_.size()) ? (is_dir_[(size_t)gj] != 0) : false;
                if (!dj) continue;

                be_eff[li] -= Ae[li][lj] * uD(gj);
                Ae_eff[li][lj] = 0.0;
            }
        }

        ImGui::Separator();
        ImGui::Text("Dirichlet on vertices: (%s, %s, %s)",
                    d0 ? "D" : "free", d1 ? "D" : "free", d2 ? "D" : "free");
        ImGui::TextUnformatted("Effective local contribution (after elimination view):");
        for (int i=0;i<3;++i) ImGui::Text("[% .3e  % .3e  % .3e]", Ae_eff[i][0], Ae_eff[i][1], Ae_eff[i][2]);
        ImGui::Text("be_eff: [% .3e  % .3e  % .3e]", be_eff[0], be_eff[1], be_eff[2]);
    }
}


} // namespace fem
