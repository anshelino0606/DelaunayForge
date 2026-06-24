#include "pde_preset.h"
#include "pde_presets.h"

namespace fem {

FEM_DEFINE_OBJECT(PDEPreset, Object, AbstractClass());
FEM_BEGIN_PROPERTY_REGISTER(PDEPreset)
{

}
FEM_END_PROPERTY_REGISTER(PDEPreset);

void PDEPreset::apply(DifferentialEquation& equation) const {
    apply_custom(equation);
}

void PDEPreset::for_each_parameter(const ForEachParameter&) const {}

PDEParameterBundleView PDEPreset::parameter_bundle() const {
    return {};
}

OperatorSpec PDEPreset::operator_spec([[maybe_unused]] const DifferentialEquation& equation) const {
    return parameter_bundle().operator_spec();
}

SolveKind PDEPreset::solve_kind() const {
    return SolveKind::Stationary;
}

SolveRequest PDEPreset::make_solve_request(const DifferentialEquation& equation) const {
    return SolveRequest{
        .model = PDEModel(equation),
        .operator_spec = operator_spec(equation),
        .discretization = {},
        .boundary = {},
        .solve_kind = solve_kind(),
        .time_step = {}
    };
}

PDEPreset* PDEPreset::default_preset() {
    return create_object<PDEPreset_Laplace>();
}

bool PDEPreset::has_exact_solution() const {
    return false;
}

bool PDEPreset::evaluate_exact_solution(
    [[maybe_unused]] double x,
    [[maybe_unused]] double y,
    [[maybe_unused]] double& u_exact,
    [[maybe_unused]] double* ux_exact,
    [[maybe_unused]] double* uy_exact
) const {
    return false;
}

const IReferenceProvider* PDEPreset::reference_provider() const {
    return nullptr;
}

bool PDEPreset::has_initial_condition() const {
    return false;
}

double PDEPreset::evaluate_initial_condition([[maybe_unused]] double x, [[maybe_unused]] double y) const {
    return 0.0;
}

bool PDEPreset::has_initial_velocity() const {
    return false;
}

double PDEPreset::evaluate_initial_velocity([[maybe_unused]] double x, [[maybe_unused]] double y) const {
    return 0.0;
}

bool PDEPreset::is_stationary() const {
    return solve_kind() == SolveKind::Stationary;
}

void PDEPreset::apply_custom([[maybe_unused]] DifferentialEquation& equation) const {}

} // namespace fem
