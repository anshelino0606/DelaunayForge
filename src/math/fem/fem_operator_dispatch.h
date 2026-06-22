#ifndef FEM_OPERATOR_DISPATCH_H
#define FEM_OPERATOR_DISPATCH_H

#include "fem_problem.h"
#include "fem_assembler.h"
#include "math/pde/operator_spec.h"
#include "math/differential_equation_solution.h"

namespace fem {

FEMSystem assemble_fractional_integral_laplacian_P1(
    const FEMProblem& P,
    const FractionalIntegralSpec& spec
);

FEMSystem assemble_fractional_regional_laplacian_P1(
    const FEMProblem& P,
    const FractionalRegionalSpec& spec
);

FEMSystem assemble_operator_P1(const FEMProblem& P, const OperatorSpec& op);

FEMSystem assemble_and_solve_operator_P1(
    const FEMProblem& P,
    const OperatorSpec& op,
    DifferentialEquationSolution& out
);

} // namespace fem

#endif // FEM_OPERATOR_DISPATCH_H
