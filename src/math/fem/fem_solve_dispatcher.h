#ifndef FEM_SOLVE_DISPATCHER_H
#define FEM_SOLVE_DISPATCHER_H

#include "math/pde/solve_request.h"
#include "math/fem/fem_assembler.h"
#include "math/fem/fem_mesh.h"
#include "math/differential_equation_solution.h"

namespace fem {

struct SolveContext {
    const FEMMesh* fem_mesh = nullptr;
};

FEMSystem solve(const SolveRequest& request, const SolveContext& context, DifferentialEquationSolution& out);
FEMSystem solve(const SolveRequest& request, const FEMMesh& mesh, DifferentialEquationSolution& out);

} // namespace fem

#endif // FEM_SOLVE_DISPATCHER_H
