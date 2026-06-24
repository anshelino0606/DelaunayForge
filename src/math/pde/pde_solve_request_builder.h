#ifndef FEM_PDE_SOLVE_REQUEST_BUILDER_H
#define FEM_PDE_SOLVE_REQUEST_BUILDER_H

#include "math/pde/boundary_model.h"
#include "math/pde/solve_request.h"

#include <span>

namespace fem {

class PDEPreset;

struct PresetSolveRequestInput {
    double time = 0.0;
    BoundaryModel boundary{};
    double dt = 0.0;
    std::span<const double> previous_state{};
    SolveKind solve_kind = SolveKind::Stationary;
};

[[nodiscard]] SolveRequest make_solve_request(
    const PDEPreset& preset,
    const PresetSolveRequestInput& input
);

} // namespace fem

#endif // FEM_PDE_SOLVE_REQUEST_BUILDER_H
