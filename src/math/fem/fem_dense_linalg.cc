#include "fem_dense_linalg.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

#if defined(__APPLE__)
  #include <Accelerate/Accelerate.h>
  #define FEM_HAS_LAPACK 1
#else
  #define FEM_HAS_LAPACK 0
#endif

namespace fem {

DenseMat::DenseMat(size_t n_) : n(n_), a(n_ * n_, 0.0) {}

double& DenseMat::operator()(size_t i, size_t j) {
    return a[i * n + j];
}

double DenseMat::operator()(size_t i, size_t j) const {
    return a[i * n + j];
}

void DenseMat::release() {
    a.clear();
    a.shrink_to_fit();
    n = 0;
}

DenseMat DenseMat::identity(size_t n) {
    DenseMat I(n);
    for (size_t i = 0; i < n; ++i) I(i, i) = 1.0;
    return I;
}

std::vector<double> matvec(const DenseMat& A, const std::vector<double>& x) {
    assert(x.size() == A.n);
    std::vector<double> y(A.n, 0.0);
    for (size_t i=0;i<A.n;++i) {
        double s = 0.0;
        const size_t row = i*A.n;
        for (size_t j=0;j<A.n;++j) s += A.a[row + j] * x[j];
        y[i] = s;
    }
    return y;
}

DenseMat transpose(const DenseMat& A) {
    DenseMat T(A.n);
    for (size_t i=0;i<A.n;++i) for (size_t j=0;j<A.n;++j) T(j,i) = A(i,j);
    return T;
}

void transpose_inplace(DenseMat& A) {
    const size_t n = A.n;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            std::swap(A(i,j), A(j,i));
}

bool cholesky_lower_inplace(DenseMat& A) {
    const size_t n = A.n;
    for (size_t i=0;i<n;++i) {
        for (size_t j=0;j<=i;++j) {
            double sum = A(i,j);
            for (size_t k=0;k<j;++k) sum -= A(i,k)*A(j,k);

            if (i == j) {
                if (sum <= 0.0) return false;
                A(i,i) = std::sqrt(sum);
            } else {
                A(i,j) = sum / A(j,j);
            }
        }
        for (size_t j=i+1;j<n;++j) A(i,j)=0.0;
    }
    return true;
}

void solve_lower(const DenseMat& L, const std::vector<double>& b, std::vector<double>& x) {
    const size_t n = L.n;
    x.assign(n, 0.0);
    for (size_t i=0;i<n;++i) {
        double s = b[i];
        for (size_t k=0;k<i;++k) s -= L(i,k)*x[k];
        x[i] = s / L(i,i);
    }
}

void solve_upper_from_lower_transpose(const DenseMat& L, const std::vector<double>& b, std::vector<double>& x) {
    const size_t n = L.n;
    x.assign(n, 0.0);
    for (int64_t i = n - 1; i >= 0;--i) {
        size_t idx = static_cast<size_t>(i);

        double s = b[idx];
        for (size_t k = idx + 1; k < n; ++k)
            s -= L(k, idx) * x[k];
        x[idx] = s / L(idx, idx);
    }
}

DenseMat solve_lower_many(const DenseMat& L, const DenseMat& B) {
    assert(L.n == B.n);
    const size_t n = L.n;
    DenseMat X(n);
    for (size_t j=0;j<n;++j) {
        for (size_t i=0;i<n;++i) {
            double s = B(i,j);
            for (size_t k=0;k<i;++k) s -= L(i,k)*X(k,j);
            X(i,j) = s / L(i,i);
        }
    }
    return X;
}

void solve_lower_many_inplace(const DenseMat& L, DenseMat& B) {
    assert(L.n == B.n);
    const size_t n = L.n;
    for (size_t j = 0; j < n; ++j) {
        for (size_t i = 0; i < n; ++i) {
            double s = B(i, j);
            for (size_t k = 0; k < i; ++k) s -= L(i, k) * B(k, j);
            B(i, j) = s / L(i, i);
        }
    }
}

DenseMat solve_upper_from_lower_transpose_many(const DenseMat& L, const DenseMat& B) {
    assert(L.n == B.n);
    const size_t n = L.n;
    DenseMat X(n);
    for (size_t j = 0; j < n; ++j) {
        for (int64_t i = n - 1; i >= 0; --i) {
            size_t idx = static_cast<size_t>(i);

            double s = B(idx,j);
            for (size_t k = idx + 1; k < n; ++k)
                s -= L(k, idx) * X(k, j);

            X(idx, j) = s / L(idx, idx);
        }
    }
    return X;
}

void solve_upper_from_lower_transpose_many_inplace(const DenseMat& L, DenseMat& B) {
    assert(L.n == B.n);
    const size_t n = L.n;
    for (size_t j = 0; j < n; ++j) {
        for (int64_t i = n - 1; i >= 0; --i) {
            size_t idx = static_cast<size_t>(i);

            double s = B(idx, j);
            for (size_t k = idx + 1; k < n; ++k)
                s -= L(k, idx) * B(k, j);

            B(idx, j) = s / L(idx, idx);
        }
    }
}


SymEig jacobi_symmetric_eig(DenseMat A, size_t max_sweeps, double tol) {
    const size_t n = A.n;
    DenseMat V = DenseMat::identity(n);

    for (size_t sweep = 0; sweep < max_sweeps; ++sweep) {
        double off = 0.0;
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j)
                off += A(i,j) * A(i,j);
        if (off < tol * tol) break;

        for (size_t p = 0; p < n; ++p) {
            for (size_t q = p + 1; q < n; ++q) {
                const double apq = A(p, q);
                if (std::abs(apq) < 1e-15) continue;

                const double app = A(p, p);
                const double aqq = A(q, q);
                const double tau = (aqq - app) / (2.0 * apq);
                const double t = (tau >= 0.0)
                    ? 1.0 / (tau + std::sqrt(1.0 + tau * tau))
                    : -1.0 / (-tau + std::sqrt(1.0 + tau * tau));
                const double c = 1.0 / std::sqrt(1.0 + t * t);
                const double s = t * c;

                for (size_t k = 0; k < n; ++k) {
                    if (k == p || k == q) continue;
                    const double akp = A(k, p);
                    const double akq = A(k, q);
                    A(k, p) = A(p, k) = c * akp - s * akq;
                    A(k, q) = A(q, k) = s * akp + c * akq;
                }

                A(p, p) = c * c * app - 2.0 * s * c * apq + s * s * aqq;
                A(q, q) = s * s * app + 2.0 * s * c * apq + c * c * aqq;
                A(p, q) = A(q, p) = 0.0;

                for (size_t k = 0; k < n; ++k) {
                    const double vkp = V(k, p);
                    const double vkq = V(k, q);
                    V(k, p) = c * vkp - s * vkq;
                    V(k, q) = s * vkp + c * vkq;
                }
            }
        }
    }

    SymEig out;
    out.eval.resize(n);
    for (size_t i = 0; i < n; ++i) out.eval[i] = A(i, i);
    A.release();

    std::vector<size_t> idx(n);
    for (size_t i = 0; i < n; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        return out.eval[a] < out.eval[b];
    });

    {
        std::vector<bool> done(n, false);
        std::vector<double> tmp_col(n);
        for (size_t i = 0; i < n; ++i) {
            if (done[i] || idx[i] == i) { done[i] = true; continue; }
            size_t j = i;
            for (size_t r = 0; r < n; ++r) tmp_col[r] = V(r, j);
            double tmp_eval = out.eval[j];
            while (true) {
                size_t src = idx[j];
                done[j] = true;
                if (src == i) {
                    for (size_t r = 0; r < n; ++r) V(r, j) = tmp_col[r];
                    out.eval[j] = tmp_eval;
                    break;
                }
                for (size_t r = 0; r < n; ++r) V(r, j) = V(r, src);
                out.eval[j] = out.eval[src];
                j = src;
            }
        }
    }

    out.evec = std::move(V);
    return out;
}


GenSymEig generalized_selfadjoint_eig_inplace(DenseMat& K, DenseMat& M) {
    assert(K.n == M.n);
    const size_t n = K.n;

#if FEM_HAS_LAPACK
    __CLPK_integer itype = 1;         // A*x = lambda*B*x
    char           jobz  = 'V';       // compute eigenvalues AND eigenvectors
    char           uplo  = 'U';       // upper triangle stored
    __CLPK_integer nn    = n;
    __CLPK_integer lda   = n;
    __CLPK_integer ldb   = n;
    __CLPK_integer info  = 0;

    std::vector<double> w(n);

    __CLPK_integer lwork = -1;
    double work_opt = 0.0;
    dsygv_(&itype, &jobz, &uplo, &nn,
           K.a.data(), &lda,
           M.a.data(), &ldb,
           w.data(), &work_opt, &lwork, &info);

    lwork = std::max((__CLPK_integer)work_opt, (__CLPK_integer)(3 * n));
    std::vector<double> work(lwork);
    dsygv_(&itype, &jobz, &uplo, &nn,
           K.a.data(), &lda,
           M.a.data(), &ldb,
           w.data(), work.data(), &lwork, &info);

    transpose_inplace(K);

    M.release();

    GenSymEig out;
    out.lambda = std::move(w);
    out.Phi    = std::move(K);
    return out;

#else
    const bool ok = cholesky_lower_inplace(M);
    assert(ok && "Mass matrix M must be SPD.");
    solve_lower_many_inplace(M, K);
    transpose_inplace(K);

    solve_lower_many_inplace(M, K);

    transpose_inplace(K);

    SymEig se = jacobi_symmetric_eig(std::move(K));

    solve_upper_from_lower_transpose_many_inplace(M, se.evec);
    M.release();

    GenSymEig out;
    out.lambda = std::move(se.eval);
    out.Phi = std::move(se.evec);
    return out;
#endif // FEM_HAS_LAPACK
}

GenSymEig generalized_selfadjoint_eig(const DenseMat& K, const DenseMat& M) {
    DenseMat K_copy = K;
    DenseMat M_copy = M;
    return generalized_selfadjoint_eig_inplace(K_copy, M_copy);
}

DenseMat dense_matmul(const DenseMat& A, const DenseMat& B) {
    assert(A.n == B.n);
    const size_t n = A.n;
    DenseMat C(n);
    for (size_t i=0;i<n;++i) {
        for (size_t k=0;k<n;++k) {
            const double aik = A(i,k);
            if (aik == 0.0) continue;
            for (size_t j=0;j<n;++j) C(i,j) += aik * B(k,j);
        }
    }
    return C;
}

std::vector<double> dense_matvec(const DenseMat& A, const std::vector<double>& x) {
    return matvec(A, x);
}

DenseMat dense_At_B(const DenseMat& A, const DenseMat& B) {
    // A^T * B
    assert(A.n == B.n);
    const size_t n = A.n;
    DenseMat C(n);
    for (size_t i=0;i<n;++i) {
        for (size_t k=0;k<n;++k) {
            const double aki = A(k,i);
            if (aki == 0.0) continue;
            for (size_t j=0;j<n;++j) C(i,j) += aki * B(k,j);
        }
    }
    return C;
}

std::vector<double> dense_At_vec(const DenseMat& A, const std::vector<double>& x) {
    assert(x.size()==A.n);
    const size_t n = A.n;
    std::vector<double> y(n, 0.0);
    for (size_t i=0;i<n;++i) {
        double s = 0.0;
        for (size_t k=0;k<n;++k) s += A(k,i) * x[k];
        y[i] = s;
    }
    return y;
}

// Solve SPD system using Cholesky (in-place factor copy).
bool solve_spd_cholesky(DenseMat A_spd, const std::vector<double>& b, std::vector<double>& x) {
    if (!cholesky_lower_inplace(A_spd)) return false;
    std::vector<double> y;
    solve_lower(A_spd, b, y);
    solve_upper_from_lower_transpose(A_spd, y, x);
    return true;
}


} // namespace fem
