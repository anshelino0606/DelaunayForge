// fem_self_tests.cc  — standalone FEM validation tests
// Robin slab (machine-precision)  +  MMS convergence (sin(π x)sin(π y))
#include "fem_self_tests.h"
#include "fem_mesh.h"
#include "fem_problem.h"
#include "fem_assembler.h"
#include "fem_solve_pipeline.h"
#include "fem_assemblers_p1.h"
#include "fem_error_analysis.h"
#include "math/differential_equation.h"
#include "math/differential_equation_solution.h"
#include <cmath>
#include <vector>
#include <sstream>
#include <iomanip>

namespace fem {

static FEMMesh make_structured_mesh(int nx, int ny, double W, double H) {
    FEMMesh m;
    const int nNode = (nx+1)*(ny+1);
    m.nodes.resize(nNode);
    for (int iy = 0; iy <= ny; ++iy)
        for (int ix = 0; ix <= nx; ++ix) {
            const int id = iy*(nx+1) + ix;
            m.nodes[id] = { ix * W / nx, iy * H / ny, id };
        }
    m.elems.reserve(2*nx*ny);
    for (int iy = 0; iy < ny; ++iy)
        for (int ix = 0; ix < nx; ++ix) {
            const int bl = iy*(nx+1) + ix;
            const int br = bl + 1;
            const int tl = bl + (nx+1);
            const int tr = tl + 1;
            // lower-left triangle
            double a1x = m.nodes[br].x - m.nodes[bl].x;
            double a1y = m.nodes[br].y - m.nodes[bl].y;
            double a2x = m.nodes[tl].x - m.nodes[bl].x;
            double a2y = m.nodes[tl].y - m.nodes[bl].y;
            double area1 = 0.5 * std::abs(a1x*a2y - a2x*a1y);
            m.elems.push_back({{bl, br, tl}, area1});
            // upper-right triangle
            double b1x = m.nodes[tl].x - m.nodes[tr].x;
            double b1y = m.nodes[tl].y - m.nodes[tr].y;
            double b2x = m.nodes[br].x - m.nodes[tr].x;
            double b2y = m.nodes[br].y - m.nodes[tr].y;
            double area2 = 0.5 * std::abs(b1x*b2y - b2x*b1y);
            m.elems.push_back({{tr, tl, br}, area2});
        }
    return m;
}

//  Robin slab self-test
//  -κ u''(x) = 0  on [0,L],  u(0) = uD0 (Dirichlet)
//  ∂u/∂n + β u = g  at x = L  (Robin, outward n = +1)
//
//  Exact:  u(x) = A x + uD0,   A = (g − β uD0) / (κ + β L)
//
//  The test uses a 2D strip [0,L]×[0,H] with two triangles.
//  P1 FEM reproduces any linear exact solution to machine precision.
RobinSlabTestResult run_robin_slab_self_test() {
    RobinSlabTestResult res;

    constexpr double kappa = 3.0;
    constexpr double L     = 2.0;
    constexpr double H     = 1.0;
    constexpr double uD0   = 1.0;
    constexpr double beta  = 5.0;
    constexpr double g_rob = 7.0;

    const double A_exact = (g_rob - beta * uD0) / (kappa + beta * L);
    res.expected_exact_slope = A_exact;

    // 1) Build 2-triangle strip mesh [0,L] × [0,H]
    FEMMesh mesh = make_structured_mesh(1, 1, L, H);

    // 2) Tag boundary edges
    //    left  (x≈0): Dirichlet u = uD0
    //    right (x≈L): Robin  ∂u/∂n + β u = g
    //    top + bottom: Neumann ∂u/∂n = 0  (natural, no action needed — skip)
    mesh.edges_bc.clear();
    for (const auto& E : mesh.elems) {
        for (int li = 0; li < 3; ++li) {
            const int ia = E.v[li];
            const int ib = E.v[(li+1)%3];
            const auto& Na = mesh.nodes[ia];
            const auto& Nb = mesh.nodes[ib];

            const double xmid = 0.5*(Na.x + Nb.x);
            const double ymid = 0.5*(Na.y + Nb.y);

            constexpr double eps = 1e-12;
            if (std::abs(xmid) < eps) {
                // left edge → Dirichlet
                FEMMesh::EdgeBC e;
                e.a = ia; e.b = ib;
                e.type = fem::BCType::Dirichlet;
                e.uD = uD0;
                mesh.edges_bc.push_back(e);
            } else if (std::abs(xmid - L) < eps) {
                // right edge → Robin
                FEMMesh::EdgeBC e;
                e.a = ia; e.b = ib;
                e.type = fem::BCType::Robin;
                e.k = beta;
                e.g = g_rob;
                mesh.edges_bc.push_back(e);
            }
            // top/bottom: natural Neumann (zero flux) — no edge needed
        }
    }

    // 3) Assemble & solve  -κ Δu = 0  with a = κ, c = 0, f = 0
    FEMProblem prob;
    prob.mesh = &mesh;
    prob.a = kappa;
    prob.c = 0.0;
    prob.f = 0.0;

    DifferentialEquationSolution sol;
    FEMSystem sys = assemble_and_solve_local_P1(prob, sol);

    if (sol.solution_u.empty()) return res;

    // 4) Check error against exact:  u_exact(x,y) = A x + uD0
    double max_err = 0.0;
    double got_A   = 0.0;
    for (int i = 0; i < mesh.dof_count(); ++i) {
        const double x = mesh.nodes[i].x;
        const double u_exact = A_exact * x + uD0;
        const double err = std::abs(sol.solution_u[i] - u_exact);
        if (err > max_err) max_err = err;
    }

    // Compute measured slope from two nodes at x=0 and x=L
    double u_left = 0, u_right = 0;
    int cnt_l = 0, cnt_r = 0;
    for (int i = 0; i < mesh.dof_count(); ++i) {
        if (mesh.nodes[i].x < 1e-12) { u_left += sol.solution_u[i]; ++cnt_l; }
        if (std::abs(mesh.nodes[i].x - L) < 1e-12) { u_right += sol.solution_u[i]; ++cnt_r; }
    }
    if (cnt_l > 0 && cnt_r > 0) got_A = (u_right / cnt_r - u_left / cnt_l) / L;

    res.max_abs_err = max_err;
    res.got_slope   = got_A;
    res.passed      = (max_err < 1e-12);
    return res;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MMS convergence study
//  -κ Δu = f  on [0,1]²,   u|_∂Ω = 0
//  u_exact = sin(πx) sin(πy)
//  f       = κ · 2π² · sin(πx) sin(πy)
//
//  Levels: N = 4, 8, 16, 32, 64
//  Expected convergence:  P1 ⟹ rate_L2 ≈ 2,  rate_H1 ≈ 1
// ═══════════════════════════════════════════════════════════════════════════════
MMSConvergenceResult run_mms_convergence_study(double kappa) {
    MMSConvergenceResult res;

    constexpr double pi = 3.14159265358979323846;
    const int levels[] = {4, 8, 16, 32, 64};
    constexpr int nLevels = 5;

    auto u_exact = [&](double x, double y) -> double {
        return std::sin(pi * x) * std::sin(pi * y);
    };
    auto grad_exact = [&](double x, double y, double& ux, double& uy) -> bool {
        ux = pi * std::cos(pi * x) * std::sin(pi * y);
        uy = pi * std::sin(pi * x) * std::cos(pi * y);
        return true;
    };

    ExactSolution exact;
    exact.has_u    = true;
    exact.has_grad = true;
    exact.u_exact   = u_exact;
    exact.grad_exact = grad_exact;

    res.levels.resize(nLevels);

    for (int lvl = 0; lvl < nLevels; ++lvl) {
        const int n = levels[lvl];
        FEMMesh mesh = make_structured_mesh(n, n, 1.0, 1.0);

        // Tag all boundary edges as Dirichlet u = 0
        mesh.edges_bc.clear();
        for (const auto& E : mesh.elems) {
            for (int li = 0; li < 3; ++li) {
                const int ia = E.v[li];
                const int ib = E.v[(li+1)%3];
                const auto& Na = mesh.nodes[ia];
                const auto& Nb = mesh.nodes[ib];

                constexpr double eps = 1e-12;
                bool on_bnd = (std::abs(Na.x) < eps && std::abs(Nb.x) < eps) ||
                              (std::abs(Na.x - 1.0) < eps && std::abs(Nb.x - 1.0) < eps) ||
                              (std::abs(Na.y) < eps && std::abs(Nb.y) < eps) ||
                              (std::abs(Na.y - 1.0) < eps && std::abs(Nb.y - 1.0) < eps);
                if (on_bnd) {
                    FEMMesh::EdgeBC e;
                    e.a = ia; e.b = ib;
                    e.type = fem::BCType::Dirichlet;
                    e.uD = 0.0;
                    mesh.edges_bc.push_back(e);
                }
            }
        }

        // Build PDE:  -κ Δu + 0 u = f  ⟹  a = κ, c = 0, f = κ 2 π² sin(πx)sin(πy)
        FEMProblem prob;
        prob.mesh = &mesh;
        prob.a = kappa;
        prob.c = 0.0;
        prob.f = [=](double x, double y) -> double {
            return kappa * 2.0 * pi * pi * std::sin(pi * x) * std::sin(pi * y);
        };

        DifferentialEquationSolution sol;
        FEMSystem sys = assemble_and_solve_local_P1(prob, sol);

        // Compute error metrics
        ErrorMetrics em = compute_error_metrics<double>(mesh, sol.solution_u, &exact);

        double h = mesh_h_max_edge<double>(mesh);

        auto& L = res.levels[lvl];
        L.n    = n;
        L.dofs = mesh.dof_count();
        L.h    = h;
        L.l2   = em.l2;
        L.h1   = em.h1_semi;

        if (lvl > 0) {
            const auto& prev = res.levels[lvl - 1];
            if (prev.l2 > 0 && L.l2 > 0 && prev.h > 0 && L.h > 0) {
                L.rate_l2 = std::log(prev.l2 / L.l2) / std::log(prev.h / L.h);
            }
            if (prev.h1 > 0 && L.h1 > 0 && prev.h > 0 && L.h > 0) {
                L.rate_h1 = std::log(prev.h1 / L.h1) / std::log(prev.h / L.h);
            }
        }
    }

    // Average rates (skip first level)
    double sum_l2 = 0, sum_h1 = 0;
    int cnt = 0;
    for (int i = 1; i < nLevels; ++i) {
        sum_l2 += res.levels[i].rate_l2;
        sum_h1 += res.levels[i].rate_h1;
        ++cnt;
    }
    if (cnt > 0) {
        res.avg_rate_l2 = sum_l2 / cnt;
        res.avg_rate_h1 = sum_h1 / cnt;
    }

    // Pass if average rates are close to theoretical
    res.passed = (res.avg_rate_l2 > 1.8 && res.avg_rate_h1 > 0.85);

    // Build CSV
    std::ostringstream csv;
    csv << std::scientific << std::setprecision(6);
    csv << "n,dofs,h,l2,h1,rate_l2,rate_h1\n";
    for (const auto& L : res.levels) {
        csv << L.n << "," << L.dofs << ","
            << L.h << "," << L.l2 << "," << L.h1 << ","
            << L.rate_l2 << "," << L.rate_h1 << "\n";
    }
    res.csv = csv.str();

    return res;
}

} // namespace fem
