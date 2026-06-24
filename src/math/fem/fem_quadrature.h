#ifndef FEM_QUADRATURE_H
#define FEM_QUADRATURE_H

#include "fem_mesh.h"

namespace fem {

struct TriQuad3 {
    static inline constexpr int n = 3;
    static inline constexpr double w[3] = {1.0/3.0, 1.0/3.0, 1.0/3.0};
    static inline constexpr double l1[3] = {2.0/3.0, 1.0/6.0, 1.0/6.0};
    static inline constexpr double l2[3] = {1.0/6.0, 2.0/3.0, 1.0/6.0};
    static inline constexpr double l3[3] = {1.0/6.0, 1.0/6.0, 2.0/3.0};
};

struct EdgeGauss2 {
    static inline constexpr int n = 2;
    static inline constexpr double xi[2] = {-0.57735026918962576451, +0.57735026918962576451};
    static inline constexpr double w[2] = {1.0, 1.0};

    [[nodiscard]] static double t(int q);
};

void tri_point(
    const FEMMesh& mesh,
    const FEMMesh::Elem& E,
    double l1,
    double l2,
    double l3,
    double& x,
    double& y
);

} // namespace fem

#endif
