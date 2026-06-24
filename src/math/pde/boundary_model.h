#ifndef FEM_PDE_BOUNDARY_MODEL_H
#define FEM_PDE_BOUNDARY_MODEL_H

#include <cstdint>
#include <vector>
#include "math/types.h"

namespace fem {

enum class BoundaryKind : uint8_t {
    None,
    Dirichlet,
    Neumann,
    Robin
};

struct BoundaryFace {
    Index a = invalid_index;
    Index b = invalid_index;
    BoundaryKind kind = BoundaryKind::None;

    // Dirichlet: u = uD.
    double uD = 0.0;

    // Neumann: a grad(u) · n = gN.
    double gN = 0.0;

    // Robin: a grad(u) · n + k u = g.
    double k = 0.0;
    double g = 0.0;
};

struct BoundaryModel {
    std::vector<BoundaryFace> faces;

    [[nodiscard]] bool empty() const noexcept { return faces.empty(); }
    [[nodiscard]] bool has_dirichlet() const noexcept {
        for (const BoundaryFace& face : faces) {
            if (face.kind == BoundaryKind::Dirichlet) return true;
        }
        return false;
    }
};

} // namespace fem

#endif // FEM_PDE_BOUNDARY_MODEL_H
