#ifndef FEM_PDE_SOLVE_REQUEST_BUILDER_H
#define FEM_PDE_SOLVE_REQUEST_BUILDER_H

#include "math/pde/pde_preset.h"
#include "math/pde/boundary_model.h"
#include "math/pde/solve_request.h"
#include "math/differential_equation.h"

#include <span>

namespace fem {

struct PresetSolveRequestInput {
    double time = 0.0;
    BoundaryModel boundary{};
    double dt = 0.0;
    std::span<const double> previous_state{};
    SolveKind solve_kind = SolveKind::Stationary;
};

[[nodiscard]] inline SolveRequest make_solve_request(
    const PDEPreset& preset,
    const PresetSolveRequestInput& input
) {
    DifferentialEquation equation;
    equation.time = input.time;
    preset.apply(equation);

    SolveRequest request = preset.make_solve_request(equation);
    request.boundary = input.boundary;
    request.solve_kind = input.solve_kind;
    request.time_step = TimeStepState{.dt = input.dt, .previous_state = input.previous_state};
    return request;
}

} // namespace fem

#endif // FEM_PDE_SOLVE_REQUEST_BUILDER_H
