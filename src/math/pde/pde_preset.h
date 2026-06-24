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

    virtual void apply(DifferentialEquation& equation) const;
    virtual void for_each_parameter(const ForEachParameter& callback) const;

    [[nodiscard]] virtual PDEParameterBundleView parameter_bundle() const;
    [[nodiscard]] virtual OperatorSpec operator_spec(const DifferentialEquation& equation) const;
    [[nodiscard]] virtual SolveKind solve_kind() const;
    [[nodiscard]] virtual SolveRequest make_solve_request(const DifferentialEquation& equation) const;

    [[nodiscard]] static PDEPreset* default_preset();

    [[nodiscard]] virtual bool has_exact_solution() const;
    [[nodiscard]] virtual bool evaluate_exact_solution(
        double x,
        double y,
        double& u_exact,
        double* ux_exact = nullptr,
        double* uy_exact = nullptr
    ) const;

    virtual const IReferenceProvider* reference_provider() const;

    [[nodiscard]] virtual bool has_initial_condition() const;
    [[nodiscard]] virtual double evaluate_initial_condition(double x, double y) const;

    [[nodiscard]] virtual bool has_initial_velocity() const;
    [[nodiscard]] virtual double evaluate_initial_velocity(double x, double y) const;

    [[nodiscard]] bool is_stationary() const;

protected:
    virtual void apply_custom(DifferentialEquation& equation) const;
};

} // namespace fem

#endif // FEM_PDE_PRESET_H
