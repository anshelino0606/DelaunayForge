#ifndef FEM_OPERATORS_NODAL_MASS_BUILDER_H
#define FEM_OPERATORS_NODAL_MASS_BUILDER_H

#include "math/fem/fem_mesh.h"
#include <vector>

namespace fem {

[[nodiscard]] std::vector<double> build_fractional_nodal_mass(const FEMMesh& mesh);

} // namespace fem

#endif // FEM_OPERATORS_NODAL_MASS_BUILDER_H
