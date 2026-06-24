#ifndef FEM_OPERATORS_FRACTIONAL_MATRIX_FREE_OPERATOR_H
#define FEM_OPERATORS_FRACTIONAL_MATRIX_FREE_OPERATOR_H

#include "math/operators/linear_operator.h"
#include "math/fem/fem_mesh.h"
#include "math/operators/exterior_interaction_model.h"
#include "math/operators/nodal_mass_builder.h"
#include "math/operators/nonlocal_kernel.h"

#include <algorithm>
#include <span>
#include <vector>

namespace fem {

struct FractionalMatrixFreeOptions {
    Real s = 0.5;
    Real scale = 1.0;
    bool include_integral_exterior_tail = false;
};

class FractionalMatrixFreeP1Operator final : public LinearOperator {
public:
    FractionalMatrixFreeP1Operator(const FEMMesh& mesh, FractionalMatrixFreeOptions options)
        : mesh_(&mesh), options_(options), kernel_{.s = options.s, .scale = options.scale}, nodal_mass_(build_fractional_nodal_mass(mesh))
    {
        if (options_.include_integral_exterior_tail) {
            exterior_diag_ = ExteriorInteractionModel{}.diagonal(mesh, nodal_mass_, options_.s, options_.scale);
        }
    }

    [[nodiscard]] Count size() const noexcept override {
        return mesh_ ? mesh_->dof_count_count() : Count{0};
    }

    void apply(std::span<const Real> x, std::span<Real> y) const override {
        const Count N_count = size();
        const Index N = static_cast<Index>(N_count);
        if (!mesh_ || x.size() < to_size(N) || y.size() < to_size(N)) return;

        std::fill(y.begin(), y.begin() + static_cast<std::ptrdiff_t>(to_size(N)), Real(0));

        for (Index i = 0; i < N; ++i) {
            const auto& lhs = mesh_->nodes[to_size(i)];
            for (Index j = i + 1; j < N; ++j) {
                const auto& rhs = mesh_->nodes[to_size(j)];
                const Real w = kernel_.pair_weight(lhs, rhs, nodal_mass_[to_size(i)], nodal_mass_[to_size(j)]);
                const Real diff = x[to_size(i)] - x[to_size(j)];
                y[to_size(i)] += w * diff;
                y[to_size(j)] -= w * diff;
            }
        }

        if (!exterior_diag_.empty()) {
            for (Index i = 0; i < N; ++i) {
                y[to_size(i)] += exterior_diag_[to_size(i)] * x[to_size(i)];
            }
        }
    }

    [[nodiscard]] Real diagonal(Index i) const noexcept override {
        if (!mesh_ || !is_valid(i, mesh_->nodes.size())) return Real(1);

        Real diag = 0.0;
        const auto& lhs = mesh_->nodes[to_size(i)];
        const Index N = mesh_->dof_count_index();
        for (Index j = 0; j < N; ++j) {
            if (i == j) continue;
            const auto& rhs = mesh_->nodes[to_size(j)];
            diag += kernel_.pair_weight(lhs, rhs, nodal_mass_[to_size(i)], nodal_mass_[to_size(j)]);
        }
        if (!exterior_diag_.empty()) diag += exterior_diag_[to_size(i)];
        return diag > Real(0) ? diag : Real(1);
    }

private:
    const FEMMesh* mesh_ = nullptr;
    FractionalMatrixFreeOptions options_{};
    NonlocalKernel kernel_{};
    std::vector<Real> nodal_mass_{};
    std::vector<Real> exterior_diag_{};
};

} // namespace fem

#endif // FEM_OPERATORS_FRACTIONAL_MATRIX_FREE_OPERATOR_H
