#ifndef FEM_ELEMENT_P1
#define FEM_ELEMENT_P1

#include <array>
#include "fem_mesh.h"

namespace fem {

struct LinearTriP1 {
    static void gradients(
        const FEMMesh& M,
        const FEMMesh::Elem& E,
        std::array<double,3>& bx,
        std::array<double,3>& by
    );

    static void stiffness(const FEMMesh& M, const FEMMesh::Elem& E, double a, double Ke[3][3]);
    static void mass(const FEMMesh::Elem& E, double rho, double Me[3][3]);
};

} // namespace fem

#endif
