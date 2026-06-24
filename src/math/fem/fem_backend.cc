#include "fem_backend.h"

#include "math/fem/fem_boundary_adapter.h"
#include "math/fem/fem_discretization_dispatch.h"
#include "math/fem/fem_problem.h"

namespace fem {

FEMSystem solve_fem(const SolveRequest& request, const FEMMesh& mesh, DifferentialEquationSolution& out) {
    SolveRequest local_request = request;
    if (local_request.boundary.empty()) {
        local_request.boundary = make_boundary_model(mesh);
    }

    FEMProblem problem(local_request, &mesh);
    return assemble_and_solve_for_basis(problem, local_request.discretization.basis, out);
}

} // namespace fem
