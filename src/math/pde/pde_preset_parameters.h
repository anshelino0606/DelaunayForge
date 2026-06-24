#ifndef FEM_PDE_PRESET_PARAMETERS_H
#define FEM_PDE_PRESET_PARAMETERS_H

#include "parameters/pde_parameter.h"
#include "math/differential_equation.h"
#include "math/pde/operator_spec.h"
#include "math/types.h"

#include <array>
#include <functional>

namespace fem {

class PDEParameterBundleView {
public:
    static constexpr Count max_parameters = 8;

    void add(PDEParameter* parameter) noexcept;
    void apply(DifferentialEquation& equation) const;
    void for_each(const std::function<void(PDEParameter*)>& callback) const;

    [[nodiscard]] OperatorSpec operator_spec() const;
    [[nodiscard]] Count size() const noexcept;

private:
    std::array<PDEParameter*, max_parameters> parameters_{};
    Count count_ = 0;
};

} // namespace fem

#endif // FEM_PDE_PRESET_PARAMETERS_H
