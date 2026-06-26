#include "fem_self_tests.h"
#include "fem_mesh.h"
#include "fem_problem.h"
#include "fem_assembler.h"
#include "fem_error_analysis.h"
#include "math/differential_equation.h"
#include "math/differential_equation_solution.h"
#include "math/math_.h"
#include "math/types.h"

#include <cmath>
#include <vector>
#include <sstream>
#include <iomanip>

namespace fem {

static FEMMesh make_structured_mesh(int nx, int ny, double W, double H) {
    FEMMesh m;

    const Index nx_i = static_cast<Index>(nx);
    const Index ny_i = static_cast<Index>(ny);

    const auto node_id = [nx_i](Index ix, Index iy) -> Index {
        return iy * (nx_i + Index{1}) + ix;
    };

    const Count nNode = static_cast<Count>((nx_i + Index{1}) * (ny_i + Index{1}));
    m.nodes.resize(nNode);

    for (Index iy = 0; iy <= ny_i; ++iy) {
        for (Index ix = 0; ix <= nx_i; ++ix) {
            const Index id = node_id(ix, iy);
            m.nodes[to_size(id)] = {
                static_cast<double>(ix) * W / static_cast<double>(nx_i),
                static_cast<double>(iy) * H / static_cast<double>(ny_i),
                id
            };
        }
    }

    m.elems.reserve(static_cast<std::size_t>(2u * nx_i * ny_i));

    for (Index iy = 0; iy < ny_i; ++iy) {
        for (Index ix = 0; ix < nx_i; ++ix) {
            const Index bl = node_id(ix, iy);
            const Index br = node_id(ix + Index{1}, iy);
            const Index tl = node_id(ix, iy + Index{1});
            const Index tr = node_id(ix + Index{1}, iy + Index{1});

            const double a1x = m.nodes[to_size(br)].x - m.nodes[to_size(bl)].x;
            const double a1y = m.nodes[to_size(br)].y - m.nodes[to_size(bl)].y;
            const double a2x = m.nodes[to_size(tl)].x - m.nodes[to_size(bl)].x;
            const double a2y = m.nodes[to_size(tl)].y - m.nodes[to_size(bl)].y;
            const double area1 = 0.5 * std::abs(a1x * a2y - a2x * a1y);
            m.elems.push_back({{bl, br, tl}, area1});

            const double b1x = m.nodes[to_size(tl)].x - m.nodes[to_size(tr)].x;
            const double b1y = m.nodes[to_size(tl)].y - m.nodes[to_size(tr)].y;
            const double b2x = m.nodes[to_size(br)].x - m.nodes[to_size(tr)].x;
            const double b2y = m.nodes[to_size(br)].y - m.nodes[to_size(tr)].y;
            const double area2 = 0.5 * std::abs(b1x * b2y - b2x * b1y);
            m.elems.push_back({{tr, tl, br}, area2});
        }
    }

    return m;
}

RobinSlabTestResult run_robin_slab_self_test() {
    RobinSlabTestResult res;

    constexpr double kappa = 3.0;
    constexpr double L = 2.0;
    constexpr double H = 1.0;
    constexpr double uD0 = 1.0;
    constexpr double beta = 5.0;
    constexpr double g_rob = 7.0;

    const double A_exact = (g_rob - beta * uD0) / (kappa + beta * L);
    res.expected_exact_slope = A_exact;

    FEMMesh mesh = make_structured_mesh(1, 1, L, H);
    mesh.edges_bc.clear();

    for (const auto& E : mesh.elems) {
        for (Index li = 0; li < Index{3}; ++li) {
            const Index ia = E.v[to_size(li)];
            const Index ib = E.v[to_size((li + Index{1}) % Index{3})];

            const auto& Na = mesh.nodes[to_size(ia)];
            const auto& Nb = mesh.nodes[to_size(ib)];

            const double xmid = 0.5 * (Na.x + Nb.x);
            constexpr double eps = 1e-12;

            if (std::abs(xmid) < eps) {
                FEMMesh::EdgeBC e;
                e.a = ia;
                e.b = ib;
                e.type = fem::BCType::Dirichlet;
                e.uD = uD0;
                mesh.edges_bc.push_back(e);
            } else if (std::abs(xmid - L) < eps) {
                FEMMesh::EdgeBC e;
                e.a = ia;
                e.b = ib;
                e.type = fem::BCType::Robin;
                e.k = beta;
                e.g = g_rob;
                mesh.edges_bc.push_back(e);
            }
        }
    }

    FEMProblem prob;
    prob.mesh = &mesh;
    prob.a = kappa;
    prob.c = 0.0;
    prob.f = 0.0;

    DifferentialEquationSolution sol;
    assemble_and_solve_local_P1(prob, sol);

    if (sol.solution_u.empty()) {
        return res;
    }

    double max_err = 0.0;
    double got_A = 0.0;

    for (Index i = 0; i < mesh.dof_count_index(); ++i) {
        const double x = mesh.nodes[to_size(i)].x;
        const double u_exact = A_exact * x + uD0;
        const double err = std::abs(sol.solution_u[to_size(i)] - u_exact);

        if (err > max_err) {
            max_err = err;
        }
    }

    double u_left = 0.0;
    double u_right = 0.0;
    Count cnt_l = 0;
    Count cnt_r = 0;

    for (Index i = 0; i < mesh.dof_count_index(); ++i) {
        if (mesh.nodes[to_size(i)].x < 1e-12) {
            u_left += sol.solution_u[to_size(i)];
            ++cnt_l;
        }

        if (std::abs(mesh.nodes[to_size(i)].x - L) < 1e-12) {
            u_right += sol.solution_u[to_size(i)];
            ++cnt_r;
        }
    }

    if (cnt_l > 0 && cnt_r > 0) {
        got_A = (u_right / static_cast<double>(cnt_r) - u_left / static_cast<double>(cnt_l)) / L;
    }

    res.max_abs_err = max_err;
    res.got_slope = got_A;
    res.passed = max_err < 1e-12;

    return res;
}

MMSConvergenceResult run_mms_convergence_study(double kappa) {
    MMSConvergenceResult res;

    const int levels[] = {4, 8, 16, 32, 64};
    constexpr int nLevels = 5;

    auto u_exact = [&](double x, double y) -> double {
        return std::sin(math::PI * x) * std::sin(math::PI * y);
    };

    auto grad_exact = [&](double x, double y, double& ux, double& uy) -> bool {
        ux = math::PI * std::cos(math::PI * x) * std::sin(math::PI * y);
        uy = math::PI * std::sin(math::PI * x) * std::cos(math::PI * y);
        return true;
    };

    ExactSolution exact;
    exact.has_u = true;
    exact.has_grad = true;
    exact.u_exact = u_exact;
    exact.grad_exact = grad_exact;

    res.levels.resize(nLevels);

    for (int lvl = 0; lvl < nLevels; ++lvl) {
        const int n = levels[lvl];
        FEMMesh mesh = make_structured_mesh(n, n, 1.0, 1.0);

        mesh.edges_bc.clear();

        for (const auto& E : mesh.elems) {
            for (Index li = 0; li < Index{3}; ++li) {
                const Index ia = E.v[to_size(li)];
                const Index ib = E.v[to_size((li + Index{1}) % Index{3})];

                const auto& Na = mesh.nodes[to_size(ia)];
                const auto& Nb = mesh.nodes[to_size(ib)];

                constexpr double eps = 1e-12;

                const bool on_bnd =
                    (std::abs(Na.x) < eps && std::abs(Nb.x) < eps) ||
                    (std::abs(Na.x - 1.0) < eps && std::abs(Nb.x - 1.0) < eps) ||
                    (std::abs(Na.y) < eps && std::abs(Nb.y) < eps) ||
                    (std::abs(Na.y - 1.0) < eps && std::abs(Nb.y - 1.0) < eps);

                if (on_bnd) {
                    FEMMesh::EdgeBC e;
                    e.a = ia;
                    e.b = ib;
                    e.type = fem::BCType::Dirichlet;
                    e.uD = 0.0;
                    mesh.edges_bc.push_back(e);
                }
            }
        }

        FEMProblem prob;
        prob.mesh = &mesh;
        prob.a = kappa;
        prob.c = 0.0;
        prob.f = [=](double x, double y) -> double {
            return kappa * 2.0 * math::PI * math::PI * std::sin(math::PI * x) * std::sin(math::PI * y);
        };

        DifferentialEquationSolution sol;
        assemble_and_solve_local_P1(prob, sol);

        ErrorMetrics em = compute_error_metrics<double>(mesh, sol.solution_u, &exact);
        const double h = mesh_h_max_edge<double>(mesh);

        auto& L = res.levels[lvl];
        L.n = n;
        L.dofs = mesh.dof_count();
        L.h = h;
        L.l2 = em.l2;
        L.h1 = em.h1_semi;

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

    double sum_l2 = 0.0;
    double sum_h1 = 0.0;
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

    res.passed = res.avg_rate_l2 > 1.8 && res.avg_rate_h1 > 0.85;

    std::ostringstream csv;
    csv << std::scientific << std::setprecision(6);
    csv << "n,dofs,h,l2,h1,rate_l2,rate_h1\n";

    for (const auto& L : res.levels) {
        csv << L.n << ","
            << L.dofs << ","
            << L.h << ","
            << L.l2 << ","
            << L.h1 << ","
            << L.rate_l2 << ","
            << L.rate_h1 << "\n";
    }

    res.csv = csv.str();

    return res;
}

} // namespace fem
