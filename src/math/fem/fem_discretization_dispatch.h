#ifndef FEM_DISCRETIZATION_DISPATCH_H
#define FEM_DISCRETIZATION_DISPATCH_H

#include "math/pde/discretization_spec.h"
#include "math/fem/fem_assembler.h"
#include "math/fem/fem_problem.h"
#include "math/differential_equation_solution.h"

namespace fem {

FEMSystem assemble_and_solve_P1(const FEMProblem& problem, DifferentialEquationSolution& out);
FEMSystem assemble_and_solve_for_basis(const FEMProblem& problem, FEMBasisKind basis, DifferentialEquationSolution& out);

} // namespace fem

#endif // FEM_DISCRETIZATION_DISPATCH_H
