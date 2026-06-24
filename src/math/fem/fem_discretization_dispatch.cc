#include "fem_discretization_dispatch.h"

#include "math/pde/operator_spec.h"

namespace fem {

FEMSystem assemble_and_solve_P1(const FEMProblem& problem, DifferentialEquationSolution& out) {
    switch (problem.solve_kind) {
        case SolveKind::HeatImplicitEuler:
            return assemble_and_solve_heat_implicit_euler_P1(problem, out);
        case SolveKind::WaveNewmark:
            return assemble_and_solve_wave_newmark_P1(problem, out);
        case SolveKind::Stationary:
        default:
            if (is_local_operator(problem.operator_spec())) {
                return assemble_and_solve_local_P1(problem, out);
            }
            return assemble_and_solve_operator_P1(problem, problem.operator_spec(), out);
    }
}

FEMSystem assemble_and_solve_for_basis(
    const FEMProblem& problem,
    FEMBasisKind basis,
    DifferentialEquationSolution& out
) {
    switch (basis) {
        case FEMBasisKind::P1:
            return assemble_and_solve_P1(problem, out);
        case FEMBasisKind::P2:
        case FEMBasisKind::Q1:
        default:
            out.invalidate();
            return {};
    }
}

} // namespace fem
