#ifndef FEM_OPERATORS_NONLOCAL_KERNEL_H
#define FEM_OPERATORS_NONLOCAL_KERNEL_H

#include "math/fem/fem_mesh.h"

namespace fem {

struct NonlocalKernel {
    double s = 0.5;
    double scale = 1.0;

    [[nodiscard]] double pair_weight(
        const FEMMesh::Node& lhs,
        const FEMMesh::Node& rhs,
        double lhs_mass,
        double rhs_mass
    ) const;
};

[[nodiscard]] double fractional_pair_weight(
    const FEMMesh::Node& lhs,
    const FEMMesh::Node& rhs,
    double lhs_mass,
    double rhs_mass,
    double s,
    double scale
);

} // namespace fem

#endif // FEM_OPERATORS_NONLOCAL_KERNEL_H
