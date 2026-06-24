#ifndef FEM_OPERATORS_NODAL_MASS_BUILDER_H
#define FEM_OPERATORS_NODAL_MASS_BUILDER_H

#include "math/fem/fem_mesh.h"
#include <vector>

namespace fem {

inline std::vector<double> build_fractional_nodal_mass(const FEMMesh& mesh) {
    std::vector<double> nodal_mass(to_size(mesh.dof_count_index()), 0.0);
    for (const auto& elem : mesh.elems) {
        const double share = elem.area / 3.0;
        for (int local = 0; local < 3; ++local) {
            const Index node = elem.v[local];
            if (is_valid(node, nodal_mass.size())) {
                nodal_mass[to_size(node)] += share;
            }
        }
    }
    return nodal_mass;
}

} // namespace fem

#endif // FEM_OPERATORS_NODAL_MASS_BUILDER_H
