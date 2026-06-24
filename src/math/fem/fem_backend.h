#ifndef FEM_BACKEND_H
#define FEM_BACKEND_H

#include "math/pde/solve_request.h"
#include "math/fem/fem_boundary_adapter.h"
#include "math/fem/fem_problem.h"
#include "math/fem/fem_assemblers_p1.h"
#include "math/differential_equation_solution.h"

namespace fem {

inline FEMSystem solve_fem(const SolveRequest& request, const FEMMesh& mesh, DifferentialEquationSolution& out) {
    SolveRequest local_request = request;
    if (local_request.boundary.empty()) {
        local_request.boundary = fem::make_boundary_model(mesh);
    }

    FEMProblem problem(local_request, &mesh);

    if (local_request.discretization.basis != FEMBasisKind::P1) {
        out.invalidate();
        return {};
    }

    switch (local_request.solve_kind) {
        case SolveKind::HeatImplicitEuler:
            return assemble_and_solve_heat_implicit_euler_P1(problem, out);
        case SolveKind::WaveNewmark:
            return assemble_and_solve_wave_newmark_P1(problem, out);
        case SolveKind::Stationary:
        default:
            if (is_local_operator(local_request.operator_spec)) {
                return assemble_and_solve_local_P1(problem, out);
            }
            return assemble_and_solve_operator_P1(problem, local_request.operator_spec, out);
    }
}

} // namespace fem

#endif // FEM_BACKEND_H
