#pragma once
#include <span>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <unordered_map>

#include "fem_mesh.h"
#include "fem_problem.h"
#include "fem_error_analysis.h"
#include "math/fem/operators/nodal_mass_builder.h"
#include "math/fem/operators/nonlocal_kernel.h"
#include "math/fem/operators/exterior_interaction_model.h"
#include "math/differential_equation.h"
#include "geom/geometry_2d.h"
#include "math/math_.h"

namespace fem {

struct EnergyMetrics {
    //  Classical:  (1/2) ∫ κ|∇u|² dΩ
    //  Fractional: (1/2) scale · Σ λ_k^s û_k²
    double bilinear_energy = 0.0;

    double robin_energy    = 0.0;   // (1/2) Σ ∫_Γ_R k·u² ds
    double reaction_energy = 0.0;   // (1/2) ∫_Ω c·u² dΩ

    double source_work     = 0.0;   // ∫_Ω f·u dΩ
    double robin_work      = 0.0;   // Σ ∫_Γ_R g·u ds
    double neumann_work    = 0.0;   // Σ ∫_Γ_N gN·u ds
    double dirichlet_work  = 0.0;   // ∫_Γ_D κ(∇u·n) u_D ds  (power injected through Dirichlet BCs)

    double total_internal  = 0.0;   // bilinear + robin_energy + reaction_energy
    double total_external  = 0.0;   // source + robin + neumann + dirichlet work

    double energy_residual = 0.0;

    //  ∫_Γ_R (k·u − g) ds   — net outward Robin "sink"
    //  Well-defined for both operators (depends only on u on Γ).
    double robin_exchange           = 0.0;
    double robin_perimeter          = 0.0;
    double robin_exchange_per_length = 0.0;

    //  k_eff = a(u,u) / (Δu / W)²
    //  WARNING: operator-dependent — not directly comparable across operators
    double k_eff_energy = 0.0;

    //  k_eff_exchange = robin_exchange / (Δu / W)
    double k_eff_exchange = 0.0;

    double integral_f  = 0.0;       // ∫ f dΩ
    double integral_cu = 0.0;       // ∫ c·u dΩ

    double u_min = 0.0;
    double u_max = 0.0;
};

inline EnergyMetrics compute_energy_terms(
    const FEMMesh& mesh,
    std::span<const double> u,
    const Coefficient<double>& c_coeff,
    const Coefficient<double>& f_coeff
) {
    EnergyMetrics em{};

    const int N = mesh.dof_count();
    if ((int)u.size() != N || mesh.nodes.empty()) return em;

    em.u_min = *std::min_element(u.begin(), u.end());
    em.u_max = *std::max_element(u.begin(), u.end());

    //   source_work = ∫ f·u dΩ
    //   reaction_energy = (1/2) ∫ c·u² dΩ
    //   integral_f = ∫ f dΩ
    //   integral_cu = ∫ c·u dΩ
    for (const auto& E : mesh.elems) {
        const auto& P0 = mesh.nodes[(size_t)E.v[0]];
        const auto& P1 = mesh.nodes[(size_t)E.v[1]];
        const auto& P2 = mesh.nodes[(size_t)E.v[2]];
        const double u0 = u[(size_t)E.v[0]];
        const double u1 = u[(size_t)E.v[1]];
        const double u2 = u[(size_t)E.v[2]];

        for (int q = 0; q < 3; ++q) {
            // 3-point quadrature (midpoint of edges)
            static constexpr double L1[3] = {0.5, 0.0, 0.5};
            static constexpr double L2[3] = {0.5, 0.5, 0.0};
            static constexpr double L3[3] = {0.0, 0.5, 0.5};
            static constexpr double wq    = 1.0 / 3.0;

            const double xq = L1[q]*P0.x + L2[q]*P1.x + L3[q]*P2.x;
            const double yq = L1[q]*P0.y + L2[q]*P1.y + L3[q]*P2.y;
            const double uq = L1[q]*u0   + L2[q]*u1   + L3[q]*u2;
            const double w  = E.area * wq;

            const double fq = f_coeff(xq, yq);
            const double cq = c_coeff(xq, yq);

            em.source_work     += w * fq * uq;
            em.reaction_energy += w * cq * uq * uq;
            em.integral_f      += w * fq;
            em.integral_cu     += w * cq * uq;
        }
    }
    em.reaction_energy *= 0.5;

    for (const auto& e : mesh.edges_bc) {
        if (!is_valid(e.a, mesh.nodes.size()) || !is_valid(e.b, mesh.nodes.size())) continue;
        

        const auto& A = mesh.nodes[to_size(e.a)];
        const auto& B = mesh.nodes[to_size(e.b)];
        const double L = std::hypot(B.x - A.x, B.y - A.y);
        if (!(L > 0.0)) continue;

        const double ua = u[to_size(e.a)];
        const double ub = u[to_size(e.b)];

        if (e.type == BCType::Robin) {
            // ∫ k·u² ds  (exact for quadratic on linear edge):
            //   = k · L/3 · (ua² + ua·ub + ub²)
            em.robin_energy += 0.5 * e.k * (L / 3.0) * (ua*ua + ua*ub + ub*ub);

            // ∫ g·u ds = g · L/2 · (ua + ub)
            em.robin_work += e.g * (L * 0.5) * (ua + ub);

            // Robin exchange: ∫ (k·u - g) ds = k·L/2·(ua+ub) - g·L
            em.robin_exchange += e.k * (L * 0.5) * (ua + ub) - e.g * L;
            em.robin_perimeter += L;

        } else if (e.type == BCType::Neumann) {
            // ∫ gN·u ds = gN · L/2 · (ua + ub)
            em.neumann_work += e.gN * (L * 0.5) * (ua + ub);
        }
    }

    em.robin_exchange_per_length = (em.robin_perimeter > 0)
        ? em.robin_exchange / em.robin_perimeter : 0.0;

    return em;
}

/// Compute the power injected through Dirichlet boundaries:
///   W_D = ∫_{Γ_D}  κ (∇u · n_out)  u_D  ds
/// Uses the P1 element gradient from the triangle adjacent to each
/// Dirichlet edge (same technique as fem_balance_metrics).
/// Returns the scalar Dirichlet work (positive = power into the domain).
template<typename Real = double>
inline double compute_dirichlet_boundary_work(
    const FEMMesh& mesh,
    std::span<const double> u,
    const Coefficient<Real>& kappa
) {
    const Count N = mesh.dof_count();
    if (to_count(u.size()) != N || mesh.nodes.empty()) return 0.0;

    // Build edge → adjacent-triangle map (same as balance_metrics)
    auto pack_edge = [](Index a, Index b) -> std::uint64_t {
        const auto lo = std::min(a, b);
        const auto hi = std::max(a, b);
        return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
    };

    struct TriVerts { Index v0, v1, v2; bool boundary = true; };
    std::unordered_map<std::uint64_t, TriVerts> adj;
    adj.reserve(mesh.elems.size() * 3);
    for (const auto& E : mesh.elems) {
        for (int e = 0; e < 3; ++e) {
            auto key = pack_edge(E.v[e], E.v[(e+1)%3]);
            auto it = adj.find(key);
            if (it == adj.end())
                adj.emplace(key, TriVerts{E.v[0], E.v[1], E.v[2], true});
            else
                it->second.boundary = false; // interior edge — mark invalid
        }
    }

    // Bounding box for outward-normal classification
    double xmin =  Math::DINF;
    double xmax = -Math::DINF;
    double ymin =  Math::DINF;
    double ymax = -Math::DINF;
    for (const auto& nd : mesh.nodes) {
        xmin = std::min(xmin, nd.x); xmax = std::max(xmax, nd.x);
        ymin = std::min(ymin, nd.y); ymax = std::max(ymax, nd.y);
    }
    const double bbmax = std::max({1.0, xmax - xmin, ymax - ymin});
    const double tol   = 1e-9 * bbmax;

    auto classify_side = [&](double ax, double ay, double bx, double by) -> int {
        // 0 = unknown, 1=left, 2=right, 3=bottom, 4=top
        if (std::abs(ax-xmin)<=tol && std::abs(bx-xmin)<=tol) return 1;
        if (std::abs(ax-xmax)<=tol && std::abs(bx-xmax)<=tol) return 2;
        if (std::abs(ay-ymin)<=tol && std::abs(by-ymin)<=tol) return 3;
        if (std::abs(ay-ymax)<=tol && std::abs(by-ymax)<=tol) return 4;
        return 0;
    };
    auto outward_normal = [](int side, double& nx, double& ny) {
        nx = ny = 0.0;
        switch (side) {
            case 1: nx = -1; break;  // left
            case 2: nx =  1; break;  // right
            case 3: ny = -1; break;  // bottom
            case 4: ny =  1; break;  // top
        }
    };

    double work = 0.0;

    for (const auto& e : mesh.edges_bc) {
        if (e.type != BCType::Dirichlet) continue;
        if (!is_valid(e.a, mesh.nodes.size()) || !is_valid(e.b, mesh.nodes.size())) continue;
        

        const auto& A = mesh.nodes[to_size(e.a)];
        const auto& B = mesh.nodes[to_size(e.b)];
        const double L = std::hypot(B.x - A.x, B.y - A.y);
        if (!(L > 0.0)) continue;

        // Find adjacent triangle
        auto key = pack_edge(e.a, e.b);
        auto it = adj.find(key);
        if (it == adj.end() || !it->second.boundary) continue;

        const Index v0 = it->second.v0, v1 = it->second.v1, v2 = it->second.v2;
        if (!is_valid(v0, mesh.nodes.size()) ||
            !is_valid(v1, mesh.nodes.size()) ||
            !is_valid(v2, mesh.nodes.size())) continue;

        const auto& P0 = mesh.nodes[to_size(v0)];
        const auto& P1 = mesh.nodes[to_size(v1)];
        const auto& P2 = mesh.nodes[to_size(v2)];

        double grad_phi[3][2];
        compute_p1_gradients<double>(P0.x, P0.y, P1.x, P1.y, P2.x, P2.y, grad_phi);

        const double gux = u[to_size(v0)]*grad_phi[0][0]
                         + u[to_size(v1)]*grad_phi[1][0]
                         + u[to_size(v2)]*grad_phi[2][0];
        const double guy = u[to_size(v0)]*grad_phi[0][1]
                         + u[to_size(v1)]*grad_phi[1][1]
                         + u[to_size(v2)]*grad_phi[2][1];

        // Outward normal
        double nx = 0.0, ny = 0.0;
        int side = classify_side(A.x, A.y, B.x, B.y);
        if (side == 0) continue;  // inner Dirichlet edge — skip
        outward_normal(side, nx, ny);

        const double mx = 0.5 * (A.x + B.x), my = 0.5 * (A.y + B.y);
        const double kappa_val = static_cast<double>(kappa(mx, my));

        // Work = ∫ κ(∇u·n) u_D ds.
        // For constant gradient on P1: flux = -κ(∇u·n_out)
        // Power INTO domain = -flux * u_D = κ(∇u·n_in) * u_D
        // But Dirichlet work = u_D * reaction = u_D * [contribution from K * u at this DOF]
        // Equivalent to: ∫ κ ∇u · n_out · u_D ds  (with sign convention of positive = injected)
        //
        // For outward-pointing n: q_out = -κ ∇u·n_out (heat flux outward is negative of conduction)
        // Power injected = -q_out * u_D * L = κ (∇u·n) * u_D * L
        const double u_D_avg = 0.5 * (u[to_size(e.a)] + u[to_size(e.b)]);  // ≈ e.uD on Dirichlet edge
        work += kappa_val * (gux * nx + guy * ny) * u_D_avg * L;
    }

    return work;
}

inline void finalize_energy_metrics(EnergyMetrics& em, double bbox_width) {
    em.total_internal = em.bilinear_energy + em.robin_energy + em.reaction_energy;
    // The variational identity tested with v=u gives:
    //   a(u,u) + r(u,u) = l(u) + W_D
    // Since total_internal uses half-energy (a/2 + r/2), total_external
    // must also carry the 1/2 factor for a consistent balance.
    em.total_external = 0.5 * (em.source_work + em.robin_work
                              + em.neumann_work + em.dirichlet_work);
    em.energy_residual = em.total_internal - em.total_external;

    const double du = std::abs(em.u_max - em.u_min);
    const double grad_proxy = (bbox_width > 1e-12 && du > 1e-12) ? (du / bbox_width) : 0.0;
    if (grad_proxy > 0.0) {
        em.k_eff_energy   = em.bilinear_energy / (grad_proxy * grad_proxy);
        em.k_eff_exchange = em.robin_exchange  / grad_proxy;
    }
}

//  Classical bilinear energy: (1/2) ∫ κ|∇u|² dΩ
//  (Element-wise constant gradient for P1.)
template<typename Real = double>
inline double compute_classical_bilinear_energy(
    const FEMMesh& mesh,
    std::span<const double> u,
    const Coefficient<Real>& kappa
) {
    double energy = 0.0;
    for (const auto& E : mesh.elems) {
        const auto& P0 = mesh.nodes[(size_t)E.v[0]];
        const auto& P1 = mesh.nodes[(size_t)E.v[1]];
        const auto& P2 = mesh.nodes[(size_t)E.v[2]];

        const double dx1 = P1.x - P0.x, dy1 = P1.y - P0.y;
        const double dx2 = P2.x - P0.x, dy2 = P2.y - P0.y;
        const double det = dx1*dy2 - dx2*dy1;
        const double inv_det = 1.0 / det;

        double grad_phi[3][2];
        grad_phi[0][0] = (P1.y - P2.y) * inv_det;
        grad_phi[0][1] = (P2.x - P1.x) * inv_det;
        grad_phi[1][0] = (P2.y - P0.y) * inv_det;
        grad_phi[1][1] = (P0.x - P2.x) * inv_det;
        grad_phi[2][0] = (P0.y - P1.y) * inv_det;
        grad_phi[2][1] = (P1.x - P0.x) * inv_det;

        double gux = 0, guy = 0;
        for (int i = 0; i < 3; ++i) {
            gux += u[(size_t)E.v[i]] * grad_phi[i][0];
            guy += u[(size_t)E.v[i]] * grad_phi[i][1];
        }

        glm::dvec2 c = Geometry2D::tri_centroid(P0.x, P0.y, P1.x, P1.y, P2.x, P2.y);

        energy += static_cast<double>(kappa(c.x, c.y)) * (gux*gux + guy*guy) * E.area;
    }
    return 0.5 * energy;
}

inline double mesh_bbox_width(const FEMMesh& mesh) {
    double xmin =  Math::DINF;
    double xmax = -Math::DINF;
    for (const auto& nd : mesh.nodes) {
        xmin = std::min(xmin, nd.x);
        xmax = std::max(xmax, nd.x);
    }
    return std::abs(xmax - xmin);
}

/// Nonlocal Dirichlet work: contribution from Dirichlet-constrained DOFs
/// to the nonlocal bilinear form.  This is the interaction between
/// interior nodes i and Dirichlet nodes j:
///   W_D = 2 Σ_{i∈int, j∈Dir} w_{ij} (u_i − u_j)·u_j
///       + Σ_{j∈Dir} [Σ_k w_{jk}] u_j²
/// which equals  a(u,u) restricted to cross-terms with Dirichlet DOFs,
/// pulled through the same node-pair kernel used in assembly.
///
/// For the half-energy convention this value is NOT halved here;
/// finalize_energy_metrics will apply the 1/2.
inline double compute_nonlocal_dirichlet_work(
    const FEMMesh& mesh,
    std::span<const double> u,
    double s,
    double C_scale = 1.0,
    bool include_exterior_tail = false
) {
    const int N = mesh.dof_count();
    if ((int)u.size() != N) return 0.0;

    // Identify Dirichlet nodes
    std::vector<bool> is_dir(N, false);
    for (const auto& e : mesh.edges_bc) {
        if (e.type == BCType::Dirichlet) {
            if (e.a >= 0 && e.a < N) is_dir[e.a] = true;
            if (e.b >= 0 && e.b < N) is_dir[e.b] = true;
        }
    }

    // Nodal masses
    std::vector<double> m = build_fractional_nodal_mass(mesh);
    double work = 0.0;

    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            const bool di = is_dir[i], dj = is_dir[j];
            if (di == dj) continue;  // both interior or both Dirichlet

            const double wij = fractional_pair_weight(mesh.nodes[i], mesh.nodes[j], m[i], m[j], s, C_scale);
            if (wij == 0.0) [[unlikely]] continue;

            const double du = u[i] - u[j];
            work += wij * du * du;
        }
    }

    if (include_exterior_tail) {
        const auto exterior_diag = approximate_integral_exterior_diagonal(mesh, m, s, C_scale);
        for (int i = 0; i < N; ++i) {
            if (is_dir[i]) {
                work += exterior_diag[static_cast<size_t>(i)] * u[static_cast<size_t>(i)] * u[static_cast<size_t>(i)];
            }
        }
    }
    return work;  // finalize_energy_metrics will apply the 1/2
}

inline double compute_fractional_bilinear_energy(
    const FEMMesh& mesh,
    std::span<const double> u,
    double s,
    double C_scale = 1.0,
    bool include_exterior_tail = false
) {
    const int N = mesh.dof_count();
    if ((int)u.size() != N) return 0.0;

    std::vector<double> m = build_fractional_nodal_mass(mesh);
    double energy = 0.0;

    for (int i = 0; i < N; ++i) {
        const auto& Pi = mesh.nodes[i];
        const double ui = u[i];
        for (int j = i + 1; j < N; ++j) {
            const double wij = fractional_pair_weight(Pi, mesh.nodes[j], m[i], m[j], s, C_scale);
            if (wij == 0.0) [[unlikely]] continue;
            const double du = ui - u[j];
            energy += wij * du * du;
        }
    }

    if (include_exterior_tail) {
        const auto exterior_diag = approximate_integral_exterior_diagonal(mesh, m, s, C_scale);
        for (int i = 0; i < N; ++i) {
            const double ui = u[static_cast<size_t>(i)];
            energy += exterior_diag[static_cast<size_t>(i)] * ui * ui;
        }
    }
    return 0.5 * energy;
}

} // namespace fem
