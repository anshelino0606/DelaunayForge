#ifndef FEM_PDE_PRESET_H
#define FEM_PDE_PRESET_H

#include "parameters/pde_rhs.h"
#include "parameters/pde_fractional_operator.h"
#include "core/object/object.h"
#include "core/object/property.h"
#include "core/macro.h"
#include "math/fem/field/fem_reference_provider.h"
#include "math/differential_equation.h"
#include "math/pde/solve_request.h"
#include "math/pde/pde_preset_parameters.h"

#include <functional>

namespace fem {

class PDEPreset : public Object {
public:
    FEM_DECLARE_OBJECT(PDEPreset);
    FEM_DECLARE_PROPERTY_REGISTER(PDEPreset);

    using ForEachParameter = std::function<void(PDEParameter*)>;

    virtual void apply(DifferentialEquation& equation) const {}
    virtual void for_each_parameter(const ForEachParameter& callback) const {}

    [[nodiscard]] virtual PDEParameterBundleView parameter_bundle() const { return {}; }

    [[nodiscard]] virtual OperatorSpec operator_spec([[maybe_unused]] const DifferentialEquation& equation) const {
        return parameter_bundle().operator_spec();
    }

    [[nodiscard]] virtual SolveKind solve_kind() const { return SolveKind::Stationary; }

    [[nodiscard]] virtual SolveRequest make_solve_request(const DifferentialEquation& equation) const {
        return SolveRequest{
            .model = PDEModel(equation),
            .operator_spec = operator_spec(equation),
            .discretization = {},
            .boundary = {},
            .solve_kind = solve_kind(),
            .time_step = {}
        };
    }

    [[nodiscard]] static PDEPreset* default_preset();

    [[nodiscard]] virtual bool has_exact_solution() const { return false; }

    [[nodiscard]] virtual bool evaluate_exact_solution(
        [[maybe_unused]] double x,
        [[maybe_unused]] double y,
        [[maybe_unused]] double& u_exact,
        [[maybe_unused]] double* ux_exact = nullptr,
        [[maybe_unused]] double* uy_exact = nullptr
    ) const {
        return false;
    }

    virtual const IReferenceProvider* reference_provider() const { return nullptr; }

    [[nodiscard]] virtual bool has_initial_condition() const { return false; }
    [[nodiscard]] virtual double evaluate_initial_condition([[maybe_unused]] double x, [[maybe_unused]] double y) const {
        return 0.0;
    }

    [[nodiscard]] virtual bool has_initial_velocity() const { return false; }
    [[nodiscard]] virtual double evaluate_initial_velocity([[maybe_unused]] double x, [[maybe_unused]] double y) const {
        return 0.0;
    }

    [[nodiscard]] bool is_stationary() const { return solve_kind() == SolveKind::Stationary; }

protected:
    virtual void apply_custom(DifferentialEquation& equation) const {}
};

} // namespace fem

#endif // FEM_PDE_PRESET_H
