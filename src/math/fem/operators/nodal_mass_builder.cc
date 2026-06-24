#include "nodal_mass_builder.h"

namespace fem {

std::vector<double> build_fractional_nodal_mass(const FEMMesh& mesh) {
    std::vector<double> nodal_mass(to_size(mesh.dof_count_index()), 0.0);
    for (const FEMMesh::Elem& elem : mesh.elems) {
        const double share = elem.area / 3.0;
        for (Index local = 0; local < Index{3}; ++local) {
            const Index node = elem.v[local];
            if (is_valid(node, nodal_mass.size())) {
                nodal_mass[to_size(node)] += share;
            }
        }
    }
    return nodal_mass;
}

} // namespace fem
