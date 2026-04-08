#ifndef FEM_QUADRATURE_H
#define FEM_QUADRATURE_H

#include "fem_mesh.h"
#include <cmath>

namespace fem {

struct TriQuad3 {
    static inline constexpr int n = 3;
    static inline constexpr double w[3]  = { 1.0/3.0, 1.0/3.0, 1.0/3.0 };

    static inline constexpr double l1[3] = { 2.0/3.0, 1.0/6.0, 1.0/6.0 };
    static inline constexpr double l2[3] = { 1.0/6.0, 2.0/3.0, 1.0/6.0 };
    static inline constexpr double l3[3] = { 1.0/6.0, 1.0/6.0, 2.0/3.0 };
};

struct EdgeGauss2 {
    static inline constexpr int n = 2;

    // hardcoded ±1/sqrt(3) and weights = 1
    static inline constexpr double xi[2] = { -0.57735026918962576451, +0.57735026918962576451 };
    static inline constexpr double w[2]  = { 1.0, 1.0 };

    // map xi in [-1,1] to t in [0,1]
    static inline double t(int q) { return 0.5 * (1.0 + xi[q]); }
};

inline void tri_point(const FEMMesh& mesh, const FEMMesh::Elem& E,
                      double l1, double l2, double l3,
                      double& x, double& y)
{
    const auto& P0 = mesh.nodes[E.v[0]];
    const auto& P1 = mesh.nodes[E.v[1]];
    const auto& P2 = mesh.nodes[E.v[2]];
    x = l1*P0.x + l2*P1.x + l3*P2.x;
    y = l1*P0.y + l2*P1.y + l3*P2.y;
}

} // namespace fem

#endif
