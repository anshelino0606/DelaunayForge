#ifndef FEM_OPERATORS_LINEAR_OPERATOR_H
#define FEM_OPERATORS_LINEAR_OPERATOR_H

#include "math/types.h"

#include <span>

namespace fem {

class LinearOperator {
public:
    virtual ~LinearOperator() = default;

    [[nodiscard]] virtual Count size() const noexcept = 0;
    virtual void apply(std::span<const Real> x, std::span<Real> y) const = 0;

    [[nodiscard]] virtual Real diagonal(Index) const noexcept { return Real(1); }
};

} // namespace fem

#endif // FEM_OPERATORS_LINEAR_OPERATOR_H
