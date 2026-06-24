#include "fem_element_p1.h"

namespace fem {

void LinearTriP1::gradients(
    const FEMMesh& M,
    const FEMMesh::Elem& E,
    std::array<double,3>& bx,
    std::array<double,3>& by
) {
    const auto& P0 = M.nodes[E.v[0]];
    const auto& P1 = M.nodes[E.v[1]];
    const auto& P2 = M.nodes[E.v[2]];

    bx[0] =  P1.y - P2.y;  by[0] = P2.x - P1.x;
    bx[1] =  P2.y - P0.y;  by[1] = P0.x - P2.x;
    bx[2] =  P0.y - P1.y;  by[2] = P1.x - P0.x;
}

void LinearTriP1::stiffness(const FEMMesh& M, const FEMMesh::Elem& E, double a, double Ke[3][3]) {
    std::array<double,3> bx{};
    std::array<double,3> by{};
    gradients(M, E, bx, by);

    const double denom = 4.0 * E.area;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Ke[i][j] = a * (bx[i] * bx[j] + by[i] * by[j]) / denom;
        }
    }
}

void LinearTriP1::mass(const FEMMesh::Elem& E, double rho, double Me[3][3]) {
    const double k = rho * E.area / 12.0;
    static const int pattern[3][3] = {{2,1,1},{1,2,1},{1,1,2}};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Me[i][j] = k * pattern[i][j];
        }
    }
}

} // namespace fem
