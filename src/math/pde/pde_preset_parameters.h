#ifndef FEM_PDE_PRESET_PARAMETERS_H
#define FEM_PDE_PRESET_PARAMETERS_H

#include "parameters/pde_parameter.h"
#include "parameters/pde_fractional_operator.h"
#include "math/differential_equation.h"
#include "math/pde/operator_spec.h"

#include <array>
#include <functional>
#include "math/types.h"

namespace fem {

class PDEParameterBundleView {
public:
    static constexpr Count max_parameters = 8;

    void add(PDEParameter* parameter) noexcept {
        if (!parameter || count_ >= max_parameters) return;
        parameters_[to_size(count_++)] = parameter;
    }

    void apply(DifferentialEquation& equation) const {
        for_each([&](PDEParameter* parameter) { parameter->apply(equation); });
    }

    void for_each(const std::function<void(PDEParameter*)>& callback) const {
        for (Index i = 0; i < count_; ++i) {
            callback(parameters_[to_size(i)]);
        }
    }

    [[nodiscard]] OperatorSpec operator_spec() const {
        OperatorSpec spec = LocalEllipticSpec{};
        for (Index i = 0; i < count_; ++i) {
            PDEParameter* parameter = parameters_[to_size(i)];
            if (const auto* fractional = dynamic_cast<const PDEParameters::FractionalOperator*>(parameter)) {
                spec = fractional->operator_spec();
            }
        }
        return spec;
    }

    [[nodiscard]] Count size() const noexcept { return count_; }

private:
    std::array<PDEParameter*, max_parameters> parameters_{};
    Count count_ = 0;
};

} // namespace fem

#endif // FEM_PDE_PRESET_PARAMETERS_H
