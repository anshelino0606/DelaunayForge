#include "fem_balance_metrics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <sstream>
#include <unordered_map>

#include "math/fem/fem_error_analysis.h"
#include "math/fem/fem_quadrature.h"
#include "math/math_.h"

namespace fem {

namespace {

static inline std::uint64_t pack_edge(Index a, Index b) noexcept {
    const auto lo = std::min(a, b);
    const auto hi = std::max(a, b);
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}

enum class Side : int { None=0, Left=1, Right=2, Bottom=3, Top=4 };

static Side classify_side(double ax, double ay, double bx, double by,
                          double xmin, double xmax, double ymin, double ymax,
                          double tol) noexcept {
    if (std::abs(ax-xmin)<=tol && std::abs(bx-xmin)<=tol) return Side::Left;
    if (std::abs(ax-xmax)<=tol && std::abs(bx-xmax)<=tol) return Side::Right;
    if (std::abs(ay-ymin)<=tol && std::abs(by-ymin)<=tol) return Side::Bottom;
    if (std::abs(ay-ymax)<=tol && std::abs(by-ymax)<=tol) return Side::Top;
    return Side::None;
}

static void outward_normal(Side s, double& nx, double& ny) noexcept {
    nx = ny = 0.0;
    switch (s) {
        case Side::Left:   nx = -1; break;
        case Side::Right:  nx =  1; break;
        case Side::Bottom: ny = -1; break;
        case Side::Top:    ny =  1; break;
        default: break;
    }
}

struct TriInfo { Index v0, v1, v2; bool boundary = true; };

} // anon

BalanceMetrics compute_balance_metrics(
    const FEMMesh& mesh,
    std::span<const double> u,
    const Coefficient<double>& a,
    const Coefficient<double>& c,
    const Coefficient<double>& f,
    const BalanceMetricsConfig& cfg)
{
    BalanceMetrics out{};
    out.dofs = mesh.dof_count();
    out.h    = mesh_h_max_edge<double>(mesh);

    if (to_count(u.size()) != mesh.dof_count() || mesh.nodes.empty() || mesh.elems.empty())
        return out;

    out.xmin = out.ymin =  Math::DINF;
    out.xmax = out.ymax = -Math::DINF;
    for (const auto& n : mesh.nodes) {
        out.xmin = std::min(out.xmin, n.x); out.xmax = std::max(out.xmax, n.x);
        out.ymin = std::min(out.ymin, n.y); out.ymax = std::max(out.ymax, n.y);
    }
    const double bbmax = std::max({1.0, std::abs(out.xmax-out.xmin), std::abs(out.ymax-out.ymin)});
    const double tol   = (cfg.outer_classify_tol > 0.0) ? cfg.outer_classify_tol : 1e-9 * bbmax;

    out.u_min = *std::min_element(u.begin(), u.end());
    out.u_max = *std::max_element(u.begin(), u.end());

    std::unordered_map<std::uint64_t, TriInfo> adj;
    adj.reserve(mesh.elems.size() * 3);
    for (std::size_t ti = 0; ti < mesh.elems.size(); ++ti) {
        const auto& E = mesh.elems[ti];
        for (int e = 0; e < 3; ++e) {
            auto key = pack_edge(E.v[e], E.v[(e+1)%3]);
            auto it = adj.find(key);
            if (it == adj.end())
                adj.emplace(key, TriInfo{E.v[0], E.v[1], E.v[2], true});
            else
                it->second.boundary = false; // interior
        }
    }

    double area = 0, int_f = 0, int_cu = 0;
    for (const auto& E : mesh.elems) {
        const auto& P0 = mesh.nodes[to_size(E.v[0])];
        const auto& P1 = mesh.nodes[to_size(E.v[1])];
        const auto& P2 = mesh.nodes[to_size(E.v[2])];
        const double u0 = u[to_size(E.v[0])], u1 = u[to_size(E.v[1])], u2 = u[to_size(E.v[2])];
        area += E.area;
        for (int q = 0; q < TriQuad3::n; ++q) {
            const double L0 = TriQuad3::l1[q], L1 = TriQuad3::l2[q], L2 = TriQuad3::l3[q];
            const double xq = L0*P0.x + L1*P1.x + L2*P2.x;
            const double yq = L0*P0.y + L1*P1.y + L2*P2.y;
            const double uq = L0*u0 + L1*u1 + L2*u2;
            const double w  = E.area * TriQuad3::w[q];
            int_f  += w * f(xq, yq);
            int_cu += w * c(xq, yq) * uq;
        }
    }
    out.domain_area = area;
    out.integral_f  = int_f;
    out.integral_cu = int_cu;

    double fl=0, fr=0, fb=0, ft=0, qi=0, il=0;
    for (const auto& e : mesh.edges_bc) {
        if (!is_valid(e.a, mesh.nodes.size()) || !is_valid(e.b, mesh.nodes.size())) continue;
        

        const auto& A = mesh.nodes[to_size(e.a)];
        const auto& B = mesh.nodes[to_size(e.b)];
        const double L = std::hypot(B.x-A.x, B.y-A.y);
        if (!(L > 0.0)) continue;

        const double mx = 0.5*(A.x+B.x), my = 0.5*(A.y+B.y);
        const double uavg = 0.5*(u[to_size(e.a)] + u[to_size(e.b)]);

        const Side side = classify_side(A.x,A.y, B.x,B.y,
                                        out.xmin,out.xmax, out.ymin,out.ymax, tol);
        const bool is_outer = (side != Side::None);

        double q_out_int = 0.0;

        if (e.type == BCType::Robin) {
            q_out_int = (e.k * uavg - e.g) * L;
        } else if (e.type == BCType::Neumann) {
            q_out_int = (-e.gN) * L;
        } else {
            // Dirichlet: reconstruct from element gradient
            auto key = pack_edge(e.a, e.b);
            auto it = adj.find(key);
            if (it == adj.end() || !it->second.boundary) continue;

            const Index v0 = it->second.v0, v1 = it->second.v1, v2 = it->second.v2;
            if (!is_valid(v0, mesh.nodes.size()) || !is_valid(v1, mesh.nodes.size()) || !is_valid(v2, mesh.nodes.size()))
                continue;

            const auto& PP0 = mesh.nodes[to_size(v0)];
            const auto& PP1 = mesh.nodes[to_size(v1)];
            const auto& PP2 = mesh.nodes[to_size(v2)];
            double grad_phi[3][2];
            compute_p1_gradients<double>(PP0.x,PP0.y, PP1.x,PP1.y, PP2.x,PP2.y, grad_phi);

            const double gux = u[to_size(v0)]*grad_phi[0][0] + u[to_size(v1)]*grad_phi[1][0] + u[to_size(v2)]*grad_phi[2][0];
            const double guy = u[to_size(v0)]*grad_phi[0][1] + u[to_size(v1)]*grad_phi[1][1] + u[to_size(v2)]*grad_phi[2][1];

            double nx = 0.0, ny = 0.0;
            if (is_outer) {
                outward_normal(side, nx, ny);
            } else {
                continue; // inner Dirichlet unusual
            }
            q_out_int = -a(mx,my) * (gux*nx + guy*ny) * L;
        }

        if (is_outer) {
            switch (side) {
                case Side::Left:   fl += q_out_int; break;
                case Side::Right:  fr += q_out_int; break;
                case Side::Bottom: fb += q_out_int; break;
                case Side::Top:    ft += q_out_int; break;
                default: break;
            }
        } else {
            qi += q_out_int;
            il += L;
        }
    }

    out.flux_left   = fl;
    out.flux_right  = fr;
    out.flux_bottom = fb;
    out.flux_top    = ft;
    out.flux_outer  = fl + fr + fb + ft;
    out.inner_exchange = qi;
    out.inner_perimeter = il;
    out.inner_exchange_per_length = (il > 0.0) ? qi / il : 0.0;

    // k_eff
    {
        const double width = std::abs(out.xmax - out.xmin);
        const double delta_u = std::abs(out.u_max - out.u_min);
        if (width > 1e-12 && delta_u > 1e-12)
            out.k_eff = std::abs(fr) / (delta_u / width);
    }

    // Conservation residual
    out.conservation_residual =
        (out.flux_outer + out.inner_exchange) + out.integral_cu - out.integral_f;

    return out;
}

std::string balance_metrics_csv_header() {
    return "level,dofs,h,"
           "flux_left,flux_right,flux_bottom,flux_top,flux_outer,"
           "inner_exchange,inner_perimeter,inner_exchange_per_length,"
           "k_eff,domain_area,integral_f,integral_cu,"
           "conservation_residual,u_min,u_max\n";
}

std::string balance_metrics_csv_row(const BalanceMetrics& m) {
    return std::format("{},{},{:.6e},"
                       "{:.6e},{:.6e},{:.6e},{:.6e},{:.6e},"
                       "{:.6e},{:.6e},{:.6e},"
                       "{:.6e},{:.6e},{:.6e},{:.6e},"
                       "{:.6e},{:.6e},{:.6e}\n",
        m.level, m.dofs, m.h,
        m.flux_left, m.flux_right, m.flux_bottom, m.flux_top, m.flux_outer,
        m.inner_exchange, m.inner_perimeter, m.inner_exchange_per_length,
        m.k_eff, m.domain_area, m.integral_f, m.integral_cu,
        m.conservation_residual, m.u_min, m.u_max);
}

} // namespace fem
