#ifndef FEM_SIMULATION_H
#define FEM_SIMULATION_H

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <functional>

#include "fem_mesh.h"
#include "fem_operator.h"
#include "fem_solver_cg.h"
#include "fem_problem.h"
#include "fem_assembler.h"
#include "fem_mesh_builder.h"
#include "geom/delaunay_mesh_generator.h"
#include "math/differential_equation.h"
#include "math/differential_equation_solution.h"

#include "fem_builders_p1.h"
#include "math/fem/fem_solve_dispatcher.h"

namespace fem {

struct FEMConfig {
    int    pde_preset = 1;     // 0: Laplace, 1: Poisson, 2: Reaction, 3: Fractional
    double a          = 1.0;   // diffusivity (local problems)
    double c          = 0.0;   // reaction coefficient (local)
    double fconst     = 1.0;   // constant RHS
    
    int    rhs_kind   = 0;     // 0 const, 1 sinus
    double A          = 1.0;
    double kx         = 0.0;
    double ky         = 0.0;

    // Robin / Newton boundary condition parameters (local only)
    double theta_over_eps = 0.0;
    double u_c            = 0.0;

    // Fractional Laplacian parameters:  (-Δ)^s u = f
    double frac_s     = 0.5;   // order s in (0,1)
    double frac_scale = 1.0;   // overall factor C_scale
};


class FEMSimulation {
public:
    FEMConfig config;

    bool ready = false;
    std::vector<double> solution_u;
    double u_min = 0.0, u_max = 0.0;

    FEMMesh mesh;
    fem::FEMSystem sys;

    void compute(const DelaunayTriangulationResult& R) {
        mesh = build_fem_mesh(R);

        SolveRequest request;
        request.model.a.set_constant(config.a);
        request.model.c.set_constant(config.pde_preset == 2 ? config.c : 0.0);

        if (config.rhs_kind == 0) {
            request.model.f.set_constant(config.fconst);
        } else {
            const double A = config.A, kx = config.kx, ky = config.ky;
            request.model.f.set_function([A, kx, ky](double x, double y) {
                return A * std::sin(kx * x) * std::sin(ky * y);
            });
        }

        // P.set_robin_newton(config.theta_over_eps, config.u_c);

        if (config.pde_preset == 3) {
            request.operator_spec = FractionalIntegralSpec{
                .s = config.frac_s,
                .scale = config.frac_scale
            };
        } else {
            request.operator_spec = LocalEllipticSpec{};
        }

        fem::DifferentialEquationSolution out;
        sys = fem::solve(request, mesh, out);

        solution_u = std::move(out.solution_u);
        u_min = out.u_min;
        u_max = out.u_max;
        ready = !solution_u.empty();
    }

    void invalidate() {
        ready = false;
        solution_u.clear();
        u_min = u_max = 0.0;
        sys = fem::FEMSystem{};
    }
};

}

#endif // FEM_SIMULATION_H
