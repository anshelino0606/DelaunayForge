#ifndef FEM_PDE_BOUNDARY_MODEL_H
#define FEM_PDE_BOUNDARY_MODEL_H

#include <vector>
#include "math/types.h"
#include "math/fem/bc_value.h"

namespace fem {

struct BoundaryFace {
    Index a = invalid_index;
    Index b = invalid_index;
    BCType kind = BCType::None;
    double uD = 0.0;
    double gN = 0.0;
    double k = 0.0;
    double g = 0.0;
};

struct BoundaryModel {
    std::vector<BoundaryFace> faces;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool has_dirichlet() const noexcept;
};

} // namespace fem

#endif // FEM_PDE_BOUNDARY_MODEL_H
