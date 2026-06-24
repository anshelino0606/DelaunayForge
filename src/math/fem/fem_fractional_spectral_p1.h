#ifndef FEM_FRACTIONAL_SPECTRAL_P1_H
#define FEM_FRACTIONAL_SPECTRAL_P1_H

#include "math/fem/fem_assembler.h"
#include "math/fem/fem_problem.h"
#include "math/differential_equation_solution.h"

namespace fem {

FEMSystem assemble_and_solve_fractional_spectral_P1(
    const FEMProblem& P,
    DifferentialEquationSolution& out
);

} // namespace fem

#endif
