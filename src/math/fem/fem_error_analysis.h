#ifndef FEM_ERROR_ANALYSIS_H
#define FEM_ERROR_ANALYSIS_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdint>

#include "math/differential_equation.h"
#include "fem_mesh.h"
#include "fem_quadrature.h"
#include "fem_assembler.h"
#include "geom/geom2d/vec.h"
#include "geom/geom2d/tri.h"
#include "geom/geom2d/types.h"

namespace fem {

// We need to think how to handle those Real stuff properly. Maybe we need to create some custom vectors and matrices
// that will take Real as template and determine this Real once in some header file. Just for consistency
template<typename Real = double>
struct TriLocatorT {
    const FEMMesh* mesh = nullptr;

    geom2d::TBoundingBox<Real> bbox;
    int32_t nx = 0, ny = 0;
    std::vector<int> cell_off;   // size = nx*ny + 1
    std::vector<int> cell_tris;  // flattened tri lists

    void reset() {
        mesh = nullptr;
        nx = ny = 0;
        cell_off.clear();
        cell_tris.clear();
    }

    void build(const FEMMesh& M, int max_dim = 128) {
        mesh = &M;
        if (M.nodes.empty() || M.elems.empty()) { reset(); mesh = &M; return; }

        for (const FEMMesh::Node& n : M.nodes) {
            bbox.update(n);
        }

        const int nt = (int)M.elems.size();
        const int base = (int)std::sqrt((double)nt);
        nx = std::clamp(base + 1, 8, max_dim);
        ny = std::clamp(base + 1, 8, max_dim);

        const Real dx = bbox.dx() / (Real)nx;
        const Real dy = bbox.dy() / (Real)ny;

        const int nc = nx * ny;
        std::vector<std::vector<int>> buckets((size_t)nc);

        for (int ti = 0; ti < nt; ++ti) {
            const FEMMesh::Elem& E = M.elems[(size_t)ti];
            const FEMMesh::Node& A = M.nodes[(size_t)E.v[0]];
            const FEMMesh::Node& B = M.nodes[(size_t)E.v[1]];
            const FEMMesh::Node& C = M.nodes[(size_t)E.v[2]];

            Real tx0 = (Real)std::min({A.x, B.x, C.x});
            Real tx1 = (Real)std::max({A.x, B.x, C.x});
            Real ty0 = (Real)std::min({A.y, B.y, C.y});
            Real ty1 = (Real)std::max({A.y, B.y, C.y});

            int ix0 = (dx > Real(0)) ? (int)std::floor((tx0 - bbox.mins.x) / dx) : 0;
            int ix1 = (dx > Real(0)) ? (int)std::floor((tx1 - bbox.mins.x) / dx) : (nx - 1);
            int iy0 = (dy > Real(0)) ? (int)std::floor((ty0 - bbox.mins.y) / dy) : 0;
            int iy1 = (dy > Real(0)) ? (int)std::floor((ty1 - bbox.mins.y) / dy) : (ny - 1);

            ix0 = std::clamp(ix0, 0, nx - 1);
            ix1 = std::clamp(ix1, 0, nx - 1);
            iy0 = std::clamp(iy0, 0, ny - 1);
            iy1 = std::clamp(iy1, 0, ny - 1);

            for (int iy = iy0; iy <= iy1; ++iy) {
                for (int ix = ix0; ix <= ix1; ++ix) {
                    buckets[(size_t)(iy * nx + ix)].push_back(ti);
                }
            }
        }

        cell_off.assign((size_t)nc + 1, 0);
        int total = 0;
        for (int c = 0; c < nc; ++c) {
            cell_off[(size_t)c] = total;
            total += (int)buckets[(size_t)c].size();
        }
        cell_off[(size_t)nc] = total;

        cell_tris.assign((size_t)total, -1);
        int at = 0;
        for (int c = 0; c < nc; ++c) {
            for (int t : buckets[(size_t)c]) cell_tris[(size_t)at++] = t;
        }
    }

    int find_triangle(Real x, Real y, Real* out_l0=nullptr, Real* out_l1=nullptr, Real* out_l2=nullptr) const noexcept {
        if (!mesh || mesh->elems.empty() || nx <= 0 || ny <= 0) return -1;

        const Real dx = bbox.dx() / (Real)nx;
        const Real dy = bbox.dy() / (Real)ny;

        int ix = (dx > Real(0)) ? (int)std::floor((x - bbox.mins.x) / dx) : 0;
        int iy = (dy > Real(0)) ? (int)std::floor((y - bbox.mins.y) / dy) : 0;
        ix = std::clamp(ix, 0, nx - 1);
        iy = std::clamp(iy, 0, ny - 1);

        const int cell = iy * nx + ix;
        const int a = cell_off[(size_t)cell];
        const int b = cell_off[(size_t)cell + 1];

        for (int k = a; k < b; ++k) {
            const int ti = cell_tris[(size_t)k];
            const FEMMesh::Elem& E = mesh->elems[(size_t)ti];
            const FEMMesh::Node& P0 = mesh->nodes[(size_t)E.v[0]];
            const FEMMesh::Node& P1 = mesh->nodes[(size_t)E.v[1]];
            const FEMMesh::Node& P2 = mesh->nodes[(size_t)E.v[2]];

            glm::dvec3 barycentrics{0.0};
            if (geom2d::tri::barycentric_in_triangle({x, y}, P0, P1, P2, barycentrics)) {
                if (out_l0) *out_l0 = barycentrics.x;
                if (out_l1) *out_l1 = barycentrics.y;
                if (out_l2) *out_l2 = barycentrics.z;
                return ti;
            }
        }

        // Fallback: brute scan (robust for points near grid boundaries).
        for (int ti = 0; ti < (int)mesh->elems.size(); ++ti) {
            const FEMMesh::Elem& E = mesh->elems[(size_t)ti];
            const FEMMesh::Node& P0 = mesh->nodes[(size_t)E.v[0]];
            const FEMMesh::Node& P1 = mesh->nodes[(size_t)E.v[1]];
            const FEMMesh::Node& P2 = mesh->nodes[(size_t)E.v[2]];

            glm::dvec3 barycentrics{0.0};
            if (geom2d::tri::barycentric_in_triangle({x, y}, P0, P1, P2, barycentrics)) {
                if (out_l0) *out_l0 = (Real)barycentrics.x;
                if (out_l1) *out_l1 = (Real)barycentrics.y;
                if (out_l2) *out_l2 = (Real)barycentrics.z;
                return ti;
            }
        }
        return -1;
    }
};

using TriLocator = TriLocatorT<double>;

template<typename Real = double>
static inline bool eval_p1_at(
    const FEMMesh& M,
    const std::vector<Real>& uh,
    Real x, Real y,
    Real& out_value,
    int* out_tri = nullptr,
    TriLocatorT<Real>* locator = nullptr
) {
    out_value = Real(0);
    if ((int)uh.size() != M.dof_count() || M.elems.empty()) return false;

    glm::dvec3 barycentrics{0.0};
    int ti = -1;
    if (locator && locator->mesh == &M && locator->nx > 0) {
        ti = locator->find_triangle(x, y, &barycentrics.x, &barycentrics.y, &barycentrics.z);
    } else {
        // brute
        for (int t = 0; t < (int)M.elems.size(); ++t) {
            const FEMMesh::Elem& E = M.elems[(size_t)t];
            const FEMMesh::Node& P0 = M.nodes[(size_t)E.v[0]];
            const FEMMesh::Node& P1 = M.nodes[(size_t)E.v[1]];
            const FEMMesh::Node& P2 = M.nodes[(size_t)E.v[2]];
            if (geom2d::tri::barycentric_in_triangle({x, y}, P0, P1, P2, barycentrics)) { 
                ti = t; 
                break; 
            }
        }
    }

    if (ti < 0) return false;
    const FEMMesh::Elem& E = M.elems[(size_t)ti];

    const Real u0 = uh[(size_t)E.v[0]];
    const Real u1 = uh[(size_t)E.v[1]];
    const Real u2 = uh[(size_t)E.v[2]];

    out_value = (Real)barycentrics.x*u0 + (Real)barycentrics.y*u1 + (Real)barycentrics.z*u2;
    if (out_tri) *out_tri = ti;
    return true;
}

#define FEM_FOREACH_EXACT_COEFF(F, Real) \
    F(u,  Real(0))                       \
    F(ux, Real(0))                       \
    F(uy, Real(0))

#define FEM_DECLARE_EXACT_COEFF(name, default_value) \
    Coefficient<Real> name{ default_value };

template<typename Real = double>
struct ExactSolutionT {
    FEM_FOREACH_EXACT_COEFF(FEM_DECLARE_EXACT_COEFF, Real)

    bool has_u    = false;
    bool has_grad = false;

    std::function<Real(Real x, Real y)> u_exact;
    std::function<bool(Real x, Real y, Real& ux, Real& uy)> grad_exact;
};

#undef FEM_DECLARE_EXACT_COEFF
#undef FEM_FOREACH_EXACT_COEFF

using ExactSolution = ExactSolutionT<double>;

template<typename Real = double>
struct ErrorMetricsT {
    bool valid = false;

    // Point diagnostics (if requested)
    bool has_point = false;
    Real x = Real(0), y = Real(0);
    Real uh = Real(0);
    Real uex = Real(0);
    Real point_abs_err = Real(0);

    // Global norms (if exact provided)
    bool has_exact = false;
    bool has_grad  = false;
    bool has_relative = false;

    Real linf_nodes = Real(0);
    Real l2 = Real(0);
    Real h1_semi = Real(0);
    Real h1_full = Real(0);

    Real linf_nodes_exact = Real(0);
    Real l2_exact = Real(0);
    Real h1_semi_exact = Real(0);
    Real h1_full_exact = Real(0);

    Real linf_nodes_rel = Real(0);
    Real l2_rel = Real(0);
    Real h1_semi_rel = Real(0);
    Real h1_full_rel = Real(0);

    bool has_energy = false;
    Real energy_A = Real(0);
};

using ErrorMetrics = ErrorMetricsT<double>;

template<typename Real>
static inline void compute_p1_gradients(
    Real x0, Real y0,
    Real x1, Real y1,
    Real x2, Real y2,
    Real grad_phi[3][2]
) {
    const Real dx1 = x1 - x0, dy1 = y1 - y0;
    const Real dx2 = x2 - x0, dy2 = y2 - y0;
    const Real det = dx1*dy2 - dx2*dy1; // = 2*Area with sign
    const Real inv_det = Real(1) / det;

    grad_phi[0][0] = (y1 - y2) * inv_det;
    grad_phi[0][1] = (x2 - x1) * inv_det;

    grad_phi[1][0] = (y2 - y0) * inv_det;
    grad_phi[1][1] = (x0 - x2) * inv_det;

    grad_phi[2][0] = (y0 - y1) * inv_det;
    grad_phi[2][1] = (x1 - x0) * inv_det;
}

template<typename Real = double>
static inline Real mesh_h_max_edge(const FEMMesh& M) {
    Real h = Real(0);
    for (const FEMMesh::Elem& E : M.elems) {
        const FEMMesh::Node& A = M.nodes[(size_t)E.v[0]];
        const FEMMesh::Node& B = M.nodes[(size_t)E.v[1]];
        const FEMMesh::Node& C = M.nodes[(size_t)E.v[2]];
        const Real ab = (Real)geom2d::vec::dist(A, B);
        const Real bc = (Real)geom2d::vec::dist(B, C);
        const Real ca = (Real)geom2d::vec::dist(C, A);
        h = std::max(h, std::max(ab, std::max(bc, ca)));
    }
    return h;
}

template<typename Real = double>
static inline ErrorMetricsT<Real> compute_error_metrics(
    const FEMMesh& M,
    const std::vector<Real>& uh,
    const ExactSolutionT<Real>* exact,                 // may be null
    const Coefficient<Real>* a_coeff = nullptr,        // optional weight for H1 (defaults to 1)
    const CRS* A_for_energy = nullptr                  // optional discrete norm
) {
    ErrorMetricsT<Real> out;
    if ((int)uh.size() != M.dof_count() || M.elems.empty()) return out;

    const bool has_exact = (exact && exact->has_u);
    const bool has_grad  = (exact && exact->has_u && exact->has_grad);
    out.has_exact = has_exact;
    out.has_grad  = has_grad;

    if (has_exact) {
        // L_infty nodal
        Real linf = Real(0);
        Real linf_exact = Real(0);
        for (int i = 0; i < M.dof_count(); ++i) {
            const auto& n = M.nodes[(size_t)i];
            const Real uex = exact->u_exact((Real)n.x, (Real)n.y);
            linf = std::max(linf, (Real)std::abs(uex - uh[(size_t)i]));
            linf_exact = std::max(linf_exact, (Real)std::abs(uex));
        }
        out.linf_nodes = linf;
        out.linf_nodes_exact = linf_exact;

        // L2 and H1
        Real l2_sq = Real(0);
        Real h1_sq = Real(0);
        Real l2_exact_sq = Real(0);
        Real h1_exact_sq = Real(0);

        for (const FEMMesh::Elem& E : M.elems) {
            const FEMMesh::Node& P0 = M.nodes[(size_t)E.v[0]];
            const FEMMesh::Node& P1 = M.nodes[(size_t)E.v[1]];
            const FEMMesh::Node& P2 = M.nodes[(size_t)E.v[2]];

            const Real x0 = (Real)P0.x, y0 = (Real)P0.y;
            const Real x1 = (Real)P1.x, y1 = (Real)P1.y;
            const Real x2 = (Real)P2.x, y2 = (Real)P2.y;

            const Real u0 = uh[(size_t)E.v[0]];
            const Real u1 = uh[(size_t)E.v[1]];
            const Real u2 = uh[(size_t)E.v[2]];

            // grad u_h is constant on element
            Real grad_phi[3][2];
            compute_p1_gradients<Real>(x0,y0,x1,y1,x2,y2,grad_phi);

            const Real gux = u0*grad_phi[0][0] + u1*grad_phi[1][0] + u2*grad_phi[2][0];
            const Real guy = u0*grad_phi[0][1] + u1*grad_phi[1][1] + u2*grad_phi[2][1];

            for (int q = 0; q < TriQuad3::n; ++q) {
                const Real L0 = (Real)TriQuad3::l1[q];
                const Real L1 = (Real)TriQuad3::l2[q];
                const Real L2 = (Real)TriQuad3::l3[q];

                const Real xq = L0*x0 + L1*x1 + L2*x2;
                const Real yq = L0*y0 + L1*y1 + L2*y2;

                const Real uhq = L0*u0 + L1*u1 + L2*u2;
                const Real ueq = exact->u_exact(xq, yq);

                const Real diff = ueq - uhq;
                const Real wq = (Real)(E.area * TriQuad3::w[q]);
                l2_sq += wq * diff * diff;
                l2_exact_sq += wq * ueq * ueq;

                if (has_grad) {
                    Real uex_x = Real(0), uex_y = Real(0);
                    exact->grad_exact(xq, yq, uex_x, uex_y);

                    const Real dx = uex_x - gux;
                    const Real dy = uex_y - guy;

                    const Real aq = a_coeff ? (*a_coeff)(xq, yq) : Real(1);
                    h1_sq += wq * aq * (dx*dx + dy*dy);
                    h1_exact_sq += wq * aq * (uex_x*uex_x + uex_y*uex_y);
                }
            }
        }

        out.l2 = std::sqrt(l2_sq);
        out.l2_exact = std::sqrt(l2_exact_sq);
        if (has_grad) {
            out.h1_semi = std::sqrt(h1_sq);
            out.h1_semi_exact = std::sqrt(h1_exact_sq);
        }
        out.h1_full = std::sqrt(out.l2 * out.l2 + out.h1_semi * out.h1_semi);
        out.h1_full_exact = std::sqrt(out.l2_exact * out.l2_exact + out.h1_semi_exact * out.h1_semi_exact);

        const Real rel_eps = Real(1e-30);
        out.linf_nodes_rel = (out.linf_nodes_exact > rel_eps) ? (out.linf_nodes / out.linf_nodes_exact) : Real(0);
        out.l2_rel = (out.l2_exact > rel_eps) ? (out.l2 / out.l2_exact) : Real(0);
        out.h1_semi_rel = (out.h1_semi_exact > rel_eps) ? (out.h1_semi / out.h1_semi_exact) : Real(0);
        out.h1_full_rel = (out.h1_full_exact > rel_eps) ? (out.h1_full / out.h1_full_exact) : Real(0);
        out.has_relative = (out.linf_nodes_exact > rel_eps) || (out.l2_exact > rel_eps) || (out.h1_full_exact > rel_eps);

        // Discrete energy norm sqrt(e^T A e) with nodal exact error vector.
        if (A_for_energy) {
            const CRS& A = *A_for_energy;
            if ((int)A.row_ptr.size() == M.dof_count() + 1) {
                std::vector<Real> e((size_t)M.dof_count(), Real(0));
                for (int i = 0; i < M.dof_count(); ++i) {
                    const auto& n = M.nodes[(size_t)i];
                    e[(size_t)i] = exact->u_exact((Real)n.x, (Real)n.y) - uh[(size_t)i];
                }

                Real eAe = Real(0);
                for (int i = 0; i < M.dof_count(); ++i) {
                    Real s = Real(0);
                    for (Index k = A.row_ptr[i]; k < A.row_ptr[i+1]; ++k) {
                        const int j = A.col_idx[k];
                        s += (Real)A.vals[k] * e[(size_t)j];
                    }
                    eAe += e[(size_t)i] * s;
                }
                out.has_energy = true;
                out.energy_A = (eAe > Real(0)) ? std::sqrt(eAe) : Real(0);
            }
        }
    }

    out.valid = true;
    return out;
}

template<typename Real = double>
static inline void compute_point_error(
    ErrorMetricsT<Real>& m,
    const FEMMesh& M,
    const std::vector<Real>& uh,
    Real x, Real y,
    const ExactSolutionT<Real>* exact,
    TriLocatorT<Real>* locator = nullptr
) {
    m.has_point = false;
    m.x = x; m.y = y;

    Real uhv = Real(0);
    if (!eval_p1_at<Real>(M, uh, x, y, uhv, nullptr, locator)) return;

    m.has_point = true;
    m.uh = uhv;

    if (exact && exact->has_u) {
        m.uex = exact->u_exact(x, y);
        m.point_abs_err = (Real)std::abs(m.uex - m.uh);
    } else {
        m.uex = Real(0);
        m.point_abs_err = Real(0);
    }
}

template<typename Real = double>
struct AitkenEstimateT {
    bool valid = false;
    Real p = Real(0);
    Real q_inf = Real(0);
    Real err_fine = Real(0);
    
    Real q1 = Real(0), q2 = Real(0), q3 = Real(0);
    Real h1 = Real(0), h2 = Real(0), h3 = Real(0);
    Real ratio_12 = Real(0), ratio_23 = Real(0);
};

using AitkenEstimate = AitkenEstimateT<double>;

template<typename Real = double>
static inline ErrorMetricsT<Real> compute_error_vs_reference(
    const FEMMesh& M_coarse,
    const std::vector<Real>& uh_coarse,
    const FEMMesh& M_fine,
    const std::vector<Real>& uh_fine,
    TriLocatorT<Real>* locator = nullptr,
    bool remove_mean_offset = false
) {
    ErrorMetricsT<Real> out;
    if ((int)uh_coarse.size() != M_coarse.dof_count() || M_coarse.elems.empty()) return out;
    if ((int)uh_fine.size() != M_fine.dof_count() || M_fine.elems.empty()) return out;

    const bool has_locator = (locator && locator->mesh == &M_fine && locator->nx > 0);

    Real mean_coarse = Real(0);
    Real mean_fine = Real(0);
    if (remove_mean_offset) {
        auto mass_lumped_mean = [](const FEMMesh& M, const std::vector<Real>& u) -> Real {
            std::vector<Real> w((size_t)M.dof_count(), Real(0));
            for (const auto& E : M.elems) {
                const Real a3 = (Real)(E.area / 3.0);
                w[(size_t)E.v[0]] += a3;
                w[(size_t)E.v[1]] += a3;
                w[(size_t)E.v[2]] += a3;
            }
            Real sw = Real(0);
            Real su = Real(0);
            for (int i = 0; i < M.dof_count(); ++i) {
                const Real wi = w[(size_t)i];
                sw += wi;
                su += wi * u[(size_t)i];
            }
            if (std::abs(sw) <= Real(1e-30)) return Real(0);
            return su / sw;
        };

        mean_coarse = mass_lumped_mean(M_coarse, uh_coarse);
        mean_fine = mass_lumped_mean(M_fine, uh_fine);
    }

    std::vector<Real> fine_gux;
    std::vector<Real> fine_guy;
    if (has_locator) {
        fine_gux.assign(M_fine.elems.size(), Real(0));
        fine_guy.assign(M_fine.elems.size(), Real(0));
        for (std::size_t ti = 0; ti < M_fine.elems.size(); ++ti) {
            const FEMMesh::Elem& E = M_fine.elems[ti];
            const FEMMesh::Node& P0 = M_fine.nodes[(size_t)E.v[0]];
            const FEMMesh::Node& P1 = M_fine.nodes[(size_t)E.v[1]];
            const FEMMesh::Node& P2 = M_fine.nodes[(size_t)E.v[2]];

            Real grad_phi[3][2];
            compute_p1_gradients<Real>((Real)P0.x,(Real)P0.y,(Real)P1.x,(Real)P1.y,(Real)P2.x,(Real)P2.y, grad_phi);

            const Real u0 = uh_fine[(size_t)E.v[0]];
            const Real u1 = uh_fine[(size_t)E.v[1]];
            const Real u2 = uh_fine[(size_t)E.v[2]];

            fine_gux[ti] = u0*grad_phi[0][0] + u1*grad_phi[1][0] + u2*grad_phi[2][0];
            fine_guy[ti] = u0*grad_phi[0][1] + u1*grad_phi[1][1] + u2*grad_phi[2][1];
        }
    }

    Real linf = Real(0);
    for (int i = 0; i < M_coarse.dof_count(); ++i) {
        const auto& n = M_coarse.nodes[(size_t)i];
        Real uh_fine_at_node = Real(0);

        if (has_locator) {
            Real l0=0, l1=0, l2=0;
            const int tf = locator->find_triangle((Real)n.x, (Real)n.y, &l0, &l1, &l2);
            if (tf >= 0) {
                const auto& Ef = M_fine.elems[(size_t)tf];
                uh_fine_at_node = l0*uh_fine[(size_t)Ef.v[0]] + l1*uh_fine[(size_t)Ef.v[1]] + l2*uh_fine[(size_t)Ef.v[2]];
                const Real dc = uh_coarse[(size_t)i] - mean_coarse;
                const Real df = uh_fine_at_node - mean_fine;
                linf = std::max(linf, (Real)std::abs(df - dc));
            }
        } else {
            if (eval_p1_at<Real>(M_fine, uh_fine, (Real)n.x, (Real)n.y, uh_fine_at_node, nullptr, locator)) {
                const Real dc = uh_coarse[(size_t)i] - mean_coarse;
                const Real df = uh_fine_at_node - mean_fine;
                linf = std::max(linf, (Real)std::abs(df - dc));
            }
        }
    }
    out.linf_nodes = linf;

    Real l2_sq = Real(0);
    Real h1_sq = Real(0);

    for (const FEMMesh::Elem& E : M_coarse.elems) {
        const FEMMesh::Node& P0 = M_coarse.nodes[(size_t)E.v[0]];
        const FEMMesh::Node& P1 = M_coarse.nodes[(size_t)E.v[1]];
        const FEMMesh::Node& P2 = M_coarse.nodes[(size_t)E.v[2]];

        const Real x0 = (Real)P0.x, y0 = (Real)P0.y;
        const Real x1 = (Real)P1.x, y1 = (Real)P1.y;
        const Real x2 = (Real)P2.x, y2 = (Real)P2.y;

        const Real u0 = uh_coarse[(size_t)E.v[0]];
        const Real u1 = uh_coarse[(size_t)E.v[1]];
        const Real u2 = uh_coarse[(size_t)E.v[2]];

        Real grad_phi[3][2];
        compute_p1_gradients<Real>(x0,y0,x1,y1,x2,y2,grad_phi);
        const Real gux = u0*grad_phi[0][0] + u1*grad_phi[1][0] + u2*grad_phi[2][0];
        const Real guy = u0*grad_phi[0][1] + u1*grad_phi[1][1] + u2*grad_phi[2][1];

        for (int q = 0; q < TriQuad3::n; ++q) {
            const Real L0 = (Real)TriQuad3::l1[q];
            const Real L1 = (Real)TriQuad3::l2[q];
            const Real L2 = (Real)TriQuad3::l3[q];

            const Real xq = L0*x0 + L1*x1 + L2*x2;
            const Real yq = L0*y0 + L1*y1 + L2*y2;

            const Real uhq = (L0*u0 + L1*u1 + L2*u2) - mean_coarse;

            Real uh_fine_q = Real(0);
            int tf = -1;
            Real lf0=0, lf1=0, lf2=0;

            if (has_locator) {
                tf = locator->find_triangle(xq, yq, &lf0, &lf1, &lf2);
                if (tf < 0) continue;
                const FEMMesh::Elem& Ef = M_fine.elems[(size_t)tf];
                uh_fine_q = (lf0*uh_fine[(size_t)Ef.v[0]] + lf1*uh_fine[(size_t)Ef.v[1]] + lf2*uh_fine[(size_t)Ef.v[2]]) - mean_fine;
            } else {
                if (!eval_p1_at<Real>(M_fine, uh_fine, xq, yq, uh_fine_q, &tf, locator)) continue;
                uh_fine_q -= mean_fine;
            }

            const Real diff = uh_fine_q - uhq;
            const Real w = (Real)(E.area * TriQuad3::w[q]);
            l2_sq += w * diff * diff;

            if (tf >= 0 && (std::size_t)tf < M_fine.elems.size()) {
                Real gux_f = Real(0), guy_f = Real(0);
                if (has_locator) {
                    gux_f = fine_gux[(size_t)tf];
                    guy_f = fine_guy[(size_t)tf];
                } else {
                    const FEMMesh::Elem& Ef = M_fine.elems[(size_t)tf];
                    const FEMMesh::Node& F0 = M_fine.nodes[(size_t)Ef.v[0]];
                    const FEMMesh::Node& F1 = M_fine.nodes[(size_t)Ef.v[1]];
                    const FEMMesh::Node& F2 = M_fine.nodes[(size_t)Ef.v[2]];
                    Real grad_phi_f[3][2];
                    compute_p1_gradients<Real>((Real)F0.x,(Real)F0.y,(Real)F1.x,(Real)F1.y,(Real)F2.x,(Real)F2.y, grad_phi_f);
                    const Real uf0 = uh_fine[(size_t)Ef.v[0]];
                    const Real uf1 = uh_fine[(size_t)Ef.v[1]];
                    const Real uf2 = uh_fine[(size_t)Ef.v[2]];
                    gux_f = uf0*grad_phi_f[0][0] + uf1*grad_phi_f[1][0] + uf2*grad_phi_f[2][0];
                    guy_f = uf0*grad_phi_f[0][1] + uf1*grad_phi_f[1][1] + uf2*grad_phi_f[2][1];
                }

                const Real dx = gux_f - gux;
                const Real dy = guy_f - guy;
                h1_sq += w * (dx*dx + dy*dy);
            }
        }
    }

    out.l2 = std::sqrt(l2_sq);
    out.h1_semi = std::sqrt(h1_sq);
    out.has_grad = true;
    out.has_exact = true;
    out.valid = true;
    return out;
}

template<typename Real = double>
static inline AitkenEstimateT<Real> aitken_estimate_3(
    Real q1, Real q2, Real q3,
    Real h1, Real h2, Real h3
) {
    AitkenEstimateT<Real> out;
    out.q1=q1; out.q2=q2; out.q3=q3;
    out.h1=h1; out.h2=h2; out.h3=h3;

    const Real tol_h = Real(1e-10);
    out.ratio_12 = h1 / std::max(h2, tol_h);
    out.ratio_23 = h2 / std::max(h3, tol_h);

    if (std::abs(h1 - h2) <= tol_h * std::max(h1, h2) &&
        std::abs(h2 - h3) <= tol_h * std::max(h2, h3)) {
        out.valid = false;
        return out;
    }

    if (h1 <= h2 || h2 <= h3) {
        out.valid = false;
        return out;
    }

    const Real ratio_scale = std::max({Real(1), out.ratio_12, out.ratio_23});
    if (std::abs(out.ratio_12 - out.ratio_23) > Real(1e-6) * ratio_scale) {
        out.valid = false;
        return out;
    }

    const Real d1 = q2 - q1;
    const Real d2 = q3 - q2;
    
    const Real scale = std::max({std::abs(q1), std::abs(q2), std::abs(q3), Real(1e-16)});
    const Real tol_q = scale * Real(1e-13);

    if (std::abs(d1) <= tol_q || std::abs(d2) <= tol_q) {
        out.q_inf = q3;
        out.err_fine = std::abs(d2);
        out.p = Real(-1);
        out.valid = false;
        return out;
    }

    // For a single leading term q(h) = q_inf + C h^p, consecutive differences
    // must have the same sign. If they flip sign, the asymptotic model is not
    // supported by the three samples and the estimate should be rejected.
    if (d1 * d2 <= Real(0)) {
        out.q_inf = q3;
        out.err_fine = std::abs(d2);
        out.p = Real(-1);
        out.valid = false;
        return out;
    }

    const Real log_r = std::log(out.ratio_12);
    if (std::abs(log_r) <= Real(1e-14)) {
        out.q_inf = q3;
        out.err_fine = std::abs(d2);
        out.p = Real(-1);
        out.valid = false;
        return out;
    }

    // q(h) = q* + C h^p:
    // p = log(|d1/d2|) / log(h1/h2)
    const Real r_abs = std::abs(d1 / d2);
    if (!(r_abs > Real(0))) {
        out.q_inf = q3;
        out.err_fine = std::abs(d2);
        out.p = Real(-1);
        out.valid = false;
        return out;
    }

    out.p = std::log(r_abs) / log_r;
    if (!std::isfinite((double)out.p)) {
        out.q_inf = q3;
        out.err_fine = std::abs(d2);
        out.p = Real(-1);
        out.valid = false;
        return out;
    }

    if (out.p < Real(0)) out.p = Real(0);
    if (out.p > Real(20)) out.p = Real(20);

    const Real rho = std::pow(out.ratio_12, out.p);
    const Real denom = rho - Real(1);
    if (std::abs(denom) <= Real(1e-12)) {
        out.q_inf = q3;
        out.err_fine = std::abs(d2);
        out.valid = false;
        return out;
    }

    out.q_inf = (rho * q3 - q2) / denom;
    out.err_fine = std::abs(out.q_inf - q3);
    out.valid = true;
    return out;
}


template<typename Real = double>
static inline std::vector<Real> compute_residual_indicators(
    const FEMMesh& M,
    const std::vector<Real>& uh,
    const Coefficient<Real>* rhs = nullptr  // RHS f(x,y)
) {
    std::vector<Real> eta((size_t)M.elems.size(), Real(0));
    
    for (size_t ti = 0; ti < M.elems.size(); ++ti) {
        const FEMMesh::Elem& E = M.elems[ti];
        const FEMMesh::Node& P0 = M.nodes[(size_t)E.v[0]];
        const FEMMesh::Node& P1 = M.nodes[(size_t)E.v[1]];
        const FEMMesh::Node& P2 = M.nodes[(size_t)E.v[2]];

        const Real x0 = (Real)P0.x, y0 = (Real)P0.y;
        const Real x1 = (Real)P1.x, y1 = (Real)P1.y;
        const Real x2 = (Real)P2.x, y2 = (Real)P2.y;

        const Real u0 = uh[(size_t)E.v[0]];
        const Real u1 = uh[(size_t)E.v[1]];
        const Real u2 = uh[(size_t)E.v[2]];

        Real grad_phi[3][2];
        compute_p1_gradients<Real>(x0,y0,x1,y1,x2,y2,grad_phi);

        const Real gux = u0*grad_phi[0][0] + u1*grad_phi[1][0] + u2*grad_phi[2][0];
        const Real guy = u0*grad_phi[0][1] + u1*grad_phi[1][1] + u2*grad_phi[2][1];
        
        Real res_l2_sq = Real(0);
        for (int q = 0; q < TriQuad3::n; ++q) {
            const Real L0 = (Real)TriQuad3::l1[q];
            const Real L1 = (Real)TriQuad3::l2[q];
            const Real L2 = (Real)TriQuad3::l3[q];

            const Real xq = L0*x0 + L1*x1 + L2*x2;
            const Real yq = L0*y0 + L1*y1 + L2*y2;

            Real fq = rhs ? (*rhs)(xq, yq) : Real(0);
            res_l2_sq += (Real)(E.area * TriQuad3::w[q]) * fq * fq;
        }

        const Real h_T = Real(2) * E.area / std::max({
            geom2d::vec::dist(P1, P0),
            geom2d::vec::dist(P2, P1),
            geom2d::vec::dist(P0, P2)
        });

        eta[ti] = h_T * h_T * res_l2_sq;
    }
    
    return eta;
}

template<typename Real = double>
static inline std::vector<Real> compute_jump_indicators(
    const FEMMesh& M,
    const std::vector<Real>& uh
) {
    std::vector<Real> eta((size_t)M.elems.size(), Real(0));
    
    for (size_t ti = 0; ti < M.elems.size(); ++ti) {
        const FEMMesh::Elem& E = M.elems[ti];
        const FEMMesh::Node& P0 = M.nodes[(size_t)E.v[0]];
        const FEMMesh::Node& P1 = M.nodes[(size_t)E.v[1]];
        const FEMMesh::Node& P2 = M.nodes[(size_t)E.v[2]];

        const Real x0 = (Real)P0.x, y0 = (Real)P0.y;
        const Real x1 = (Real)P1.x, y1 = (Real)P1.y;
        const Real x2 = (Real)P2.x, y2 = (Real)P2.y;

        const Real u0 = uh[(size_t)E.v[0]];
        const Real u1 = uh[(size_t)E.v[1]];
        const Real u2 = uh[(size_t)E.v[2]];

        Real grad_phi[3][2];
        compute_p1_gradients<Real>(x0,y0,x1,y1,x2,y2,grad_phi);

        const Real gux = u0*grad_phi[0][0] + u1*grad_phi[1][0] + u2*grad_phi[2][0];
        const Real guy = u0*grad_phi[0][1] + u1*grad_phi[1][1] + u2*grad_phi[2][1];

        const Real e01 = std::hypot(x1 - x0, y1 - y0);
        const Real e12 = std::hypot(x2 - x1, y2 - y1);
        const Real e20 = std::hypot(x0 - x2, y0 - y2);

        const Real nx01 = (y1 - y0) / e01;
        const Real ny01 = -(x1 - x0) / e01;

        const Real nx12 = (y2 - y1) / e12;
        const Real ny12 = -(x2 - x1) / e12;

        const Real nx20 = (y0 - y2) / e20;
        const Real ny20 = -(x0 - x2) / e20;

        Real jump_sq = Real(0);
        jump_sq += e01 * (gux*nx01 + guy*ny01) * (gux*nx01 + guy*ny01);
        jump_sq += e12 * (gux*nx12 + guy*ny12) * (gux*nx12 + guy*ny12);
        jump_sq += e20 * (gux*nx20 + guy*ny20) * (gux*nx20 + guy*ny20);

        eta[ti] = jump_sq;
    }
    
    return eta;
}

} // namespace fem

#endif // FEM_ERROR_ANALYSIS_H
