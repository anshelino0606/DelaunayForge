#include "fem_solver_cg.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <span>

namespace fem {

void crs_matvec(const CRS& A, const double* x, double* y, int n) noexcept {
    for (int i = 0; i < n; ++i) {
        double s = 0.0;
        const Index rb = A.row_ptr[to_size(static_cast<Index>(i))];
        const Index re = A.row_ptr[to_size(static_cast<Index>(i + 1))];
        for (Index k = rb; k < re; ++k) {
            s += A.vals[to_size(k)] * x[to_size(A.col_idx[to_size(k)])];
        }
        y[i] = s;
    }
}

void crs_matvec(const CRS& A, const std::vector<double>& x, std::vector<double>& y) {
    const int n = static_cast<int>(A.row_ptr.size()) - 1;
    y.assign(static_cast<std::size_t>(n), 0.0);
    crs_matvec(A, x.data(), y.data(), n);
}

int FEMSolverCG::solve(const LinearOperator& A, const std::vector<double>& b, std::vector<double>& x) {
    const Count n_count = A.size();
    const Index n = static_cast<Index>(n_count);
    if (n == 0 || b.size() != to_size(n)) return 0;
    if (x.size() != b.size()) x.assign(b.size(), 0.0);

    std::vector<double> inv_diag(to_size(n), 1.0);
    bool use_precond = true;
    for (Index i = 0; i < n; ++i) {
        const double diag_val = A.diagonal(i);
        if (diag_val <= 0.0 || !std::isfinite(diag_val)) { use_precond = false; break; }
        inv_diag[to_size(i)] = 1.0 / diag_val;
    }

    std::vector<double> r(to_size(n));
    std::vector<double> z(to_size(n));
    std::vector<double> p(to_size(n));
    std::vector<double> Ap(to_size(n));

    A.apply(std::span<const double>(x.data(), x.size()), std::span<double>(Ap.data(), Ap.size()));
    for (Index i = 0; i < n; ++i) r[to_size(i)] = b[to_size(i)] - Ap[to_size(i)];

    const double bnorm2 = std::inner_product(b.begin(), b.end(), b.begin(), 0.0);
    const double tol2 = tol * tol * (bnorm2 > 0.0 ? bnorm2
               : std::inner_product(r.begin(), r.end(), r.begin(), 0.0));

    if (use_precond) {
        for (Index i = 0; i < n; ++i) z[to_size(i)] = inv_diag[to_size(i)] * r[to_size(i)];
    } else {
        z = r;
    }

    p = z;
    double rz = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);

    if (std::inner_product(r.begin(), r.end(), r.begin(), 0.0) <= tol2) return 0;

    for (int it = 0; it < max_it; ++it) {
        A.apply(std::span<const double>(p.data(), p.size()), std::span<double>(Ap.data(), Ap.size()));
        const double pAp = std::inner_product(p.begin(), p.end(), Ap.begin(), 0.0);
        if (!std::isfinite(pAp) || std::abs(pAp) <= 1e-30) return it;

        const double alpha = rz / pAp;
        if (!std::isfinite(alpha)) return it;

        for (Index i = 0; i < n; ++i) x[to_size(i)] += alpha * p[to_size(i)];
        for (Index i = 0; i < n; ++i) r[to_size(i)] -= alpha * Ap[to_size(i)];

        const double rnorm2 = std::inner_product(r.begin(), r.end(), r.begin(), 0.0);
        if (!std::isfinite(rnorm2)) return it + 1;
        if (rnorm2 <= tol2) return it + 1;

        if (use_precond) {
            for (Index i = 0; i < n; ++i) z[to_size(i)] = inv_diag[to_size(i)] * r[to_size(i)];
        } else {
            z = r;
        }

        const double rz_new = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);
        const double beta = rz_new / rz;
        rz = rz_new;

        for (Index i = 0; i < n; ++i) p[to_size(i)] = z[to_size(i)] + beta * p[to_size(i)];
    }
    return max_it;
}

int FEMSolverCG::solve(const CRS& A, const std::vector<double>& b, std::vector<double>& x) {
    const int n = static_cast<int>(b.size());
    if (n == 0) return 0;

    std::vector<double> inv_diag(static_cast<std::size_t>(n), 1.0);
    bool use_precond = true;
    for (int i = 0; i < n; ++i) {
        double diag_val = 0.0;
        for (Index k = A.row_ptr[to_size(static_cast<Index>(i))]; k < A.row_ptr[to_size(static_cast<Index>(i + 1))]; ++k) {
            if (A.col_idx[to_size(k)] == static_cast<Index>(i)) { diag_val = A.vals[to_size(k)]; break; }
        }
        if (diag_val <= 0.0) { use_precond = false; break; }
        inv_diag[static_cast<std::size_t>(i)] = 1.0 / diag_val;
    }

    std::vector<double> r(static_cast<std::size_t>(n));
    std::vector<double> z(static_cast<std::size_t>(n));
    std::vector<double> p(static_cast<std::size_t>(n));
    std::vector<double> Ap(static_cast<std::size_t>(n));

    crs_matvec(A, x.data(), Ap.data(), n);
    for (int i = 0; i < n; ++i) r[static_cast<std::size_t>(i)] = b[static_cast<std::size_t>(i)] - Ap[static_cast<std::size_t>(i)];

    const double bnorm2 = std::inner_product(b.begin(), b.end(), b.begin(), 0.0);
    const double tol2 = tol * tol * (bnorm2 > 0.0 ? bnorm2
               : std::inner_product(r.begin(), r.end(), r.begin(), 0.0));

    if (use_precond) {
        for (int i = 0; i < n; ++i) z[static_cast<std::size_t>(i)] = inv_diag[static_cast<std::size_t>(i)] * r[static_cast<std::size_t>(i)];
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

        for (int i = 0; i < n; ++i) x[static_cast<std::size_t>(i)] += alpha * p[static_cast<std::size_t>(i)];
        for (int i = 0; i < n; ++i) r[static_cast<std::size_t>(i)] -= alpha * Ap[static_cast<std::size_t>(i)];

        const double rnorm2 = std::inner_product(r.begin(), r.end(), r.begin(), 0.0);
        if (!std::isfinite(rnorm2)) return it + 1;
        if (rnorm2 <= tol2) return it + 1;

        if (use_precond) {
            for (int i = 0; i < n; ++i) z[static_cast<std::size_t>(i)] = inv_diag[static_cast<std::size_t>(i)] * r[static_cast<std::size_t>(i)];
        } else {
            z = r;
        }

        const double rz_new = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);
        const double beta = rz_new / rz;
        rz = rz_new;

        for (int i = 0; i < n; ++i) p[static_cast<std::size_t>(i)] = z[static_cast<std::size_t>(i)] + beta * p[static_cast<std::size_t>(i)];
    }
    return max_it;
}

} // namespace fem
