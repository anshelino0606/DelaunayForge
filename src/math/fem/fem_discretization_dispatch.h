#ifndef FEM_DISCRETIZATION_DISPATCH_H
#define FEM_DISCRETIZATION_DISPATCH_H

#include "math/pde/discretization_spec.h"
#include "math/pde/operator_spec.h"
#include "math/fem/fem_assembler.h"
#include "math/fem/fem_problem.h"
#include "math/differential_equation_solution.h"

namespace fem {

inline FEMSystem assemble_and_solve_P1(const FEMProblem& problem, DifferentialEquationSolution& out) {
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

inline FEMSystem assemble_and_solve_for_basis(
    const FEMProblem& problem,
    FEMBasisKind basis,
    DifferentialEquationSolution& out
) {
    switch (basis) {
        case FEMBasisKind::P1:
            return assemble_and_solve_P1(problem, out);
        default:
            out.invalidate();
            return {};
    }
}

} // namespace fem

#endif // FEM_DISCRETIZATION_DISPATCH_H
