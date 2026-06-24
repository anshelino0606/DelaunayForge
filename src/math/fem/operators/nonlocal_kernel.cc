#include "nonlocal_kernel.h"

#include <cmath>

namespace fem {

double NonlocalKernel::pair_weight(
    const FEMMesh::Node& lhs,
    const FEMMesh::Node& rhs,
    double lhs_mass,
    double rhs_mass
) const {
    const double dx = lhs.x - rhs.x;
    const double dy = lhs.y - rhs.y;
    const double r2 = dx * dx + dy * dy;
    if (r2 == 0.0) return 0.0;

    return scale * lhs_mass * rhs_mass / std::pow(r2, 1.0 + s);
}

double fractional_pair_weight(
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
