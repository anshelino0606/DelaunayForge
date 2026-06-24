#include "pde_solve_request_builder.h"

#include "math/pde/pde_preset.h"
#include "math/differential_equation.h"

namespace fem {

SolveRequest make_solve_request(
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
