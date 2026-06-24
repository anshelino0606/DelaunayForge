#include "fem_quadrature.h"

namespace fem {

double EdgeGauss2::t(int q) {
    return 0.5 * (1.0 + xi[q]);
}

void tri_point(
    const FEMMesh& mesh,
    const FEMMesh::Elem& E,
    double l1,
    double l2,
    double l3,
    double& x,
    double& y
) {
    const auto& P0 = mesh.nodes[E.v[0]];
    const auto& P1 = mesh.nodes[E.v[1]];
    const auto& P2 = mesh.nodes[E.v[2]];
    x = l1 * P0.x + l2 * P1.x + l3 * P2.x;
    y = l1 * P0.y + l2 * P1.y + l3 * P2.y;
}

} // namespace fem
