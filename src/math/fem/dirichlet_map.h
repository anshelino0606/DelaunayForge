#ifndef DIRICHLET_MAP
#define DIRICHLET_MAP

#include "fem_mesh.h"
#include "math/fem/fem_boundary_adapter.h"

namespace fem {

[[nodiscard]] inline DirichletData build_dirichlet_data(const BoundaryModel& boundary, int dof_count) {
    const Count N = dof_count < 0 ? Count{0} : static_cast<Count>(dof_count);
    return build_dirichlet_mask(boundary, N);
}

[[nodiscard]] inline DirichletData build_dirichlet_data(const FEMMesh& mesh) {
    return build_dirichlet_mask(mesh);
}

} // namespace fem

#endif // DIRICHLET_MAP
