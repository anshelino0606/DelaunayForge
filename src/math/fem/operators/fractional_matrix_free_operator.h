#ifndef FEM_OPERATORS_FRACTIONAL_MATRIX_FREE_OPERATOR_H
#define FEM_OPERATORS_FRACTIONAL_MATRIX_FREE_OPERATOR_H

#include "math/operators/linear_operator.h"
#include "math/fem/fem_mesh.h"
#include "math/fem/operators/nonlocal_kernel.h"

#include <vector>

namespace fem {

struct FractionalMatrixFreeOptions {
    Real s = 0.5;
    Real scale = 1.0;
    bool include_integral_exterior_tail = false;
};

class FractionalMatrixFreeP1Operator final : public LinearOperator {
public:
    FractionalMatrixFreeP1Operator(const FEMMesh& mesh, FractionalMatrixFreeOptions options);

    [[nodiscard]] Count size() const noexcept override;
    void apply(std::span<const Real> x, std::span<Real> y) const override;
    [[nodiscard]] Real diagonal(Index i) const noexcept override;

private:
    const FEMMesh* mesh_ = nullptr;
    FractionalMatrixFreeOptions options_{};
    NonlocalKernel kernel_{};
    std::vector<Real> nodal_mass_{};
    std::vector<Real> exterior_diag_{};
};

} // namespace fem

#endif // FEM_OPERATORS_FRACTIONAL_MATRIX_FREE_OPERATOR_H
