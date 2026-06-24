#ifndef PDE_FRACTIONAL_OPERATOR_H
#define PDE_FRACTIONAL_OPERATOR_H

#include "pde_parameter.h"
#include "math/fractional_equation_config.h"
#include "math/differential_equation.h"
#include "math/pde/operator_spec.h"
#include "core/object/object.h"
#include "core/object/property.h"
#include "core/macro.h"
#include "core/object/property_attribute.h"

namespace fem::PDEParameters {

class alignas(64) FractionalOperator : public PDEParameter {
public:
    FEM_DECLARE_OBJECT(FractionalOperator);
    FEM_DECLARE_PROPERTY_REGISTER(FractionalOperator);

    FractionalOperator() = default;
    ~FractionalOperator() override = default;

    [[nodiscard]] FractionalEquationConfig config() const;
    [[nodiscard]] OperatorSpec operator_spec() const;
    void apply(DifferentialEquation& equation) const override;

private:
    double s_ = 0.5;
    double scale_ = 1.0;
    double eig_clip_ = 0.0;
    FractionalType type_ = FractionalType::Integral;
    int spectral_k_ = -1;
};

} // namespace fem::PDEParameters

#endif
