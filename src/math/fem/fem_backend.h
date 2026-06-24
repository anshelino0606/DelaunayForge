#ifndef FEM_BACKEND_H
#define FEM_BACKEND_H

#include "math/pde/solve_request.h"
#include "math/fem/fem_assembler.h"
#include "math/fem/fem_mesh.h"
#include "math/differential_equation_solution.h"

namespace fem {

FEMSystem solve_fem(const SolveRequest& request, const FEMMesh& mesh, DifferentialEquationSolution& out);

} // namespace fem

#endif // FEM_BACKEND_H
