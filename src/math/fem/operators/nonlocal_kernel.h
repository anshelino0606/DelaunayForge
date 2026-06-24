#ifndef FEM_OPERATORS_NONLOCAL_KERNEL_H
#define FEM_OPERATORS_NONLOCAL_KERNEL_H

#include "math/fem/fem_mesh.h"
#include <cmath>

namespace fem {

struct NonlocalKernel {
    double s = 0.5;
    double scale = 1.0;

    [[nodiscard]] double pair_weight(
        const FEMMesh::Node& lhs,
        const FEMMesh::Node& rhs,
        double lhs_mass,
        double rhs_mass
    ) const {
        const double dx = lhs.x - rhs.x;
        const double dy = lhs.y - rhs.y;
        const double r2 = dx * dx + dy * dy;
        if (r2 == 0.0) return 0.0;

        // 2D kernel |x-y|^{-(2+2s)} written against r².
        return scale * lhs_mass * rhs_mass / std::pow(r2, 1.0 + s);
    }
};

inline double fractional_pair_weight(
    const FEMMesh::Node& lhs,
    const FEMMesh::Node& rhs,
    double lhs_mass,
    double rhs_mass,
    double s,
    double scale
) {
    return NonlocalKernel{.s = s, .scale = scale}.pair_weight(lhs, rhs, lhs_mass, rhs_mass);
}

} // namespace fem

#endif // FEM_OPERATORS_NONLOCAL_KERNEL_H
