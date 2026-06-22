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

    [[nodiscard]] FractionalEquationConfig config() const {
        return FractionalEquationConfig{
            .s = s_,
            .scale = scale_,
            .eig_clip = eig_clip_,
            .type = type_,
            .spectral_k = spectral_k_
        };
    }

    [[nodiscard]] OperatorSpec operator_spec() const {
        return make_operator_spec(config());
    }

    void apply([[maybe_unused]] DifferentialEquation& equation) const override {}

private:
    double s_ = 0.5;               ///< Fractional order ∈ (0, 1)
    double scale_ = 1.0;           ///< Operator scaling factor
    double eig_clip_ = 0.0;        ///< Eigenvalue clipping threshold
    FractionalType type_ = FractionalType::Integral;  ///< Operator type
    int spectral_k_ = -1;                             ///< Modal truncation
};

} // namespace fem::PDEParameters

#endif
