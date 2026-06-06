#ifndef FEM_SOLVER_CG
#define FEM_SOLVER_CG

#include "fem_mesh.h"
#include "core/macro.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace fem {

inline void crs_matvec(const CRS& A,
                       const double* FEM_RESTRICT x,
                       double* FEM_RESTRICT y,
                       int n) noexcept {
    for (int i = 0; i < n; ++i) {
        double s = 0.0;
        const int rb = A.row_ptr[i];
        const int re = A.row_ptr[i + 1];
        for (int k = rb; k < re; ++k)
            s += A.vals[k] * x[A.col_idx[k]];
        y[i] = s;
    }
}

inline void crs_matvec(const CRS& A, const std::vector<double>& x, std::vector<double>& y) {
    const int n = (int)A.row_ptr.size() - 1;
    y.assign((size_t)n, 0.0);
    crs_matvec(A, x.data(), y.data(), n);
}

struct FEMSolverCG {
    int max_it = 2000;
    double tol = 1e-8;

    int solve(const CRS& A, const std::vector<double>& b, std::vector<double>& x) {
        const int n = (int)b.size();
        if (n == 0) return 0;

        std::vector<double> inv_diag((size_t)n, 1.0);
        bool use_precond = true;
        for (int i = 0; i < n; ++i) {
            double diag_val = 0.0;
            for (int k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) {
                if (A.col_idx[k] == i) { diag_val = A.vals[k]; break; }
            }
            if (diag_val <= 0.0) { use_precond = false; break; }
            inv_diag[(size_t)i] = 1.0 / diag_val;
        }

        std::vector<double> r((size_t)n);
        std::vector<double> z((size_t)n);
        std::vector<double> p((size_t)n);
        std::vector<double> Ap((size_t)n);

        crs_matvec(A, x.data(), Ap.data(), n);
        for (int i = 0; i < n; ++i) r[(size_t)i] = b[(size_t)i] - Ap[(size_t)i];

        const double bnorm2 = std::inner_product(b.begin(), b.end(), b.begin(), 0.0);
        const double tol2 = tol * tol * (bnorm2 > 0.0 ? bnorm2
                   : std::inner_product(r.begin(), r.end(), r.begin(), 0.0));

        if (use_precond) {
            for (int i = 0; i < n; ++i) z[(size_t)i] = inv_diag[(size_t)i] * r[(size_t)i];
        } else {
            z = r;
        }

        p = z;
        double rz = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);

        if (std::inner_product(r.begin(), r.end(), r.begin(), 0.0) <= tol2) return 0;

        for (int it = 0; it < max_it; ++it) {
            crs_matvec(A, p.data(), Ap.data(), n);
            const double pAp = std::inner_product(p.begin(), p.end(), Ap.begin(), 0.0);
            if (!std::isfinite(pAp) || std::abs(pAp) <= 1e-30) return it;

            const double alpha = rz / pAp;
            if (!std::isfinite(alpha)) return it;

            for (int i = 0; i < n; ++i) x[(size_t)i] += alpha * p[(size_t)i];
            for (int i = 0; i < n; ++i) r[(size_t)i] -= alpha * Ap[(size_t)i];

            const double rnorm2 = std::inner_product(r.begin(), r.end(), r.begin(), 0.0);
            if (!std::isfinite(rnorm2)) return it + 1;
            if (rnorm2 <= tol2) return it + 1;

            if (use_precond) {
                for (int i = 0; i < n; ++i) z[(size_t)i] = inv_diag[(size_t)i] * r[(size_t)i];
            } else {
                z = r;
            }

            const double rz_new = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);
            const double beta = rz_new / rz;
            rz = rz_new;

            for (int i = 0; i < n; ++i) p[(size_t)i] = z[(size_t)i] + beta * p[(size_t)i];
        }
        return max_it;
    }
};

}

#endif
