#ifndef FEM_SOLVE_DISPATCHER_H
#define FEM_SOLVE_DISPATCHER_H

#include "math/pde/solve_request.h"
#include "math/fem/fem_mesh.h"
#include "math/fem/fem_backend.h"
#include "math/differential_equation_solution.h"

namespace fem {

struct SolveContext {
    const FEMMesh* fem_mesh = nullptr;
};

inline FEMSystem solve(const SolveRequest& request, const SolveContext& context, DifferentialEquationSolution& out) {
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

inline FEMSystem solve(const SolveRequest& request, const FEMMesh& mesh, DifferentialEquationSolution& out) {
    return solve(request, SolveContext{.fem_mesh = &mesh}, out);
}

} // namespace fem

#endif // FEM_SOLVE_DISPATCHER_H
