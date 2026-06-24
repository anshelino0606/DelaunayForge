#include "fem_simulation.h"

#include "fem_mesh_builder.h"
#include "math/differential_equation_solution.h"
#include "math/fem/fem_solve_dispatcher.h"
#include "math/pde/operator_spec.h"
#include "math/pde/solve_request.h"

#include <cmath>
#include <utility>

namespace fem {

void FEMSimulation::compute(const DelaunayTriangulationResult& R) {
    mesh = build_fem_mesh(R);

    SolveRequest request;
    request.model.a.set_constant(config.a);
    request.model.c.set_constant(config.pde_preset == 2 ? config.c : 0.0);

    if (config.rhs_kind == 0) {
        request.model.f.set_constant(config.fconst);
    } else {
        const double A = config.A;
        const double kx = config.kx;
        const double ky = config.ky;
        request.model.f.set_function([A, kx, ky](double x, double y) {
            return A * std::sin(kx * x) * std::sin(ky * y);
        });
    }

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

void FEMSimulation::invalidate() {
    ready = false;
    solution_u.clear();
    u_min = 0.0;
    u_max = 0.0;
    sys = fem::FEMSystem{};
}

} // namespace fem
