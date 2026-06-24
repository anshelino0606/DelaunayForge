#include "fem_solve_dispatcher.h"

#include "math/fem/fem_backend.h"

namespace fem {

FEMSystem solve(const SolveRequest& request, const SolveContext& context, DifferentialEquationSolution& out) {
    switch (request.discretization.backend) {
        case DiscretizationBackend::FEM:
        default:
            if (!context.fem_mesh) {
                out.invalidate();
                return {};
            }
            return solve_fem(request, *context.fem_mesh, out);
    }
}

FEMSystem solve(const SolveRequest& request, const FEMMesh& mesh, DifferentialEquationSolution& out) {
    return solve(request, SolveContext{.fem_mesh = &mesh}, out);
}

} // namespace fem
