#include "boundary_model.h"

namespace fem {

bool BoundaryModel::empty() const noexcept {
    return faces.empty();
}

bool BoundaryModel::has_dirichlet() const noexcept {
    for (const BoundaryFace& face : faces) {
        if (face.kind == BCType::Dirichlet) return true;
    }
    return false;
}

} // namespace fem
