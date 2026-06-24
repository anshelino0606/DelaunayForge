#ifndef FEM_BOUNDARY_ADAPTER_H
#define FEM_BOUNDARY_ADAPTER_H

#include "math/pde/boundary_model.h"
#include "math/fem/fem_mesh.h"
#include "math/fem/bc_value.h"
#include "math/types.h"

#include <cstdint>
#include <vector>

namespace fem {

[[nodiscard]] BoundaryModel make_boundary_model(const FEMMesh& mesh);

struct DirichletMask {
    std::vector<std::uint8_t> is_dirichlet;
    std::vector<Real> value;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] Count size() const noexcept;
    [[nodiscard]] bool contains(Index node) const noexcept;
};

[[nodiscard]] DirichletMask build_dirichlet_mask(const BoundaryModel& boundary, Count dof_count);
[[nodiscard]] DirichletMask build_dirichlet_mask(const FEMMesh& mesh);

} // namespace fem

#endif // FEM_BOUNDARY_ADAPTER_H
