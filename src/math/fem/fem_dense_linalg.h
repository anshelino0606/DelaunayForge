#ifndef FEM_DENSE_LINALG_H
#define FEM_DENSE_LINALG_H

#include <vector>

namespace fem {

struct DenseMat {
    size_t n = 0;
    std::vector<double> a;

    DenseMat() = default;
    explicit DenseMat(size_t n_);

    double& operator()(size_t i, size_t j);
    double operator()(size_t i, size_t j) const;

    void release();
    static DenseMat identity(size_t n);
};

std::vector<double> matvec(const DenseMat& A, const std::vector<double>& x);
DenseMat transpose(const DenseMat& A);
void transpose_inplace(DenseMat& A);
bool cholesky_lower_inplace(DenseMat& A);
void solve_lower(const DenseMat& L, const std::vector<double>& b, std::vector<double>& x);
void solve_upper_from_lower_transpose(const DenseMat& L, const std::vector<double>& b, std::vector<double>& x);
DenseMat solve_lower_many(const DenseMat& L, const DenseMat& B);
void solve_lower_many_inplace(const DenseMat& L, DenseMat& B);
DenseMat solve_upper_from_lower_transpose_many(const DenseMat& L, const DenseMat& B);
void solve_upper_from_lower_transpose_many_inplace(const DenseMat& L, DenseMat& B);

struct SymEig {
    std::vector<double> eval;
    DenseMat evec;
};

SymEig jacobi_symmetric_eig(DenseMat A, size_t max_sweeps = 200, double tol = 1e-12);

struct GenSymEig {
    std::vector<double> lambda;
    DenseMat Phi;
};

GenSymEig generalized_selfadjoint_eig_inplace(DenseMat& K, DenseMat& M);
GenSymEig generalized_selfadjoint_eig(const DenseMat& K, const DenseMat& M);
DenseMat dense_matmul(const DenseMat& A, const DenseMat& B);
std::vector<double> dense_matvec(const DenseMat& A, const std::vector<double>& x);
DenseMat dense_At_B(const DenseMat& A, const DenseMat& B);
std::vector<double> dense_At_vec(const DenseMat& A, const std::vector<double>& x);
bool solve_spd_cholesky(DenseMat A_spd, const std::vector<double>& b, std::vector<double>& x);

} // namespace fem

#endif
