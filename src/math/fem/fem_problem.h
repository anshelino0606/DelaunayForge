#ifndef FEM_PROBLEM
#define FEM_PROBLEM

#include "fem_mesh.h"
#include "math/differential_equation.h"
#include <functional>
#include <vector>
#include <span>

namespace fem {

struct FEMProblem : public fem::DifferentialEquation {
    const FEMMesh* mesh = nullptr;

    double dt = 0.0;
    std::span<const double> u_prev;  // Empty span = no previous solution
};

}

#endif
