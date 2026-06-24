#include "pde_preset_parameters.h"

#include "parameters/pde_fractional_operator.h"

namespace fem {

void PDEParameterBundleView::add(PDEParameter* parameter) noexcept {
    if (!parameter || count_ >= max_parameters) return;
    parameters_[to_size(count_++)] = parameter;
}

void PDEParameterBundleView::apply(DifferentialEquation& equation) const {
    for_each([&](PDEParameter* parameter) { parameter->apply(equation); });
}

void PDEParameterBundleView::for_each(const std::function<void(PDEParameter*)>& callback) const {
    for (Index i = 0; i < count_; ++i) {
        callback(parameters_[to_size(i)]);
    }
}

OperatorSpec PDEParameterBundleView::operator_spec() const {
    OperatorSpec spec = LocalEllipticSpec{};
    for (Index i = 0; i < count_; ++i) {
        PDEParameter* parameter = parameters_[to_size(i)];
        const PDEParameters::FractionalOperator* fractional = dynamic_cast<const PDEParameters::FractionalOperator*>(parameter);
        if (fractional) {
            spec = fractional->operator_spec();
        }
    }
    return spec;
}

Count PDEParameterBundleView::size() const noexcept {
    return count_;
}

} // namespace fem
