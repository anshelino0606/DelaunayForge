#ifndef FEM_MESH_BUILDER
#define FEM_MESH_BUILDER

#include "fem_mesh.h"
#include "delaunay_types.h"

#include <functional>

namespace fem {

[[nodiscard]] FEMMesh build_fem_mesh(const DelaunayTriangulationResult& R);
[[nodiscard]] FEMMesh build_fem_mesh_all_boundary_dirichlet(
    const DelaunayTriangulationResult& R,
    const std::function<double(double, double)>& u_dirichlet
);

} // namespace fem

#endif // FEM_MESH_BUILDER
