#ifndef FEM_SOLVER_CG
#define FEM_SOLVER_CG

#include "fem_mesh.h"
#include "math/operators/linear_operator.h"

#include <vector>

namespace fem {

void crs_matvec(const CRS& A, const double* x, double* y, int n) noexcept;
void crs_matvec(const CRS& A, const std::vector<double>& x, std::vector<double>& y);

struct FEMSolverCG {
    int max_it = 2000;
    double tol = 1e-8;

    int solve(const LinearOperator& A, const std::vector<double>& b, std::vector<double>& x);
    int solve(const CRS& A, const std::vector<double>& b, std::vector<double>& x);
};

} // namespace fem

#endif
