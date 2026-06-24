#ifndef FEM_ASSEMBLER
#define FEM_ASSEMBLER

#include "fem_problem.h"
#include "fem_element_p1.h"
#include <tuple>
#include <vector>
#include <utility>
#include "math/differential_equation_solution.h"
#include "math/types.h"

namespace fem {

inline constexpr double kDefaultSolveTol   = 1e-8;
inline constexpr int    kDefaultSolveMaxIt = 2000;


template<typename Scalar = Real>
struct FEMSystemT {
    CRS A;
    std::vector<Scalar> b;
    std::vector<Scalar> x;
};

using FEMSystem = FEMSystemT<double>;



CRS build_crs_from_triplets(Index n, std::vector<Triplet> T);
inline CRS build_crs_from_triplets(int n, std::vector<Triplet> T) {
    return build_crs_from_triplets(n < 0 ? Index{0} : static_cast<Index>(n), std::move(T));
}

FEMSystem assemble_poisson_P1(const FEMProblem& P);

FEMSystem assemble_heat_implicit_euler_P1(const FEMProblem& P);

FEMSystem assemble_wave_newmark_P1(const FEMProblem& P);

FEMSystem assemble_fractional_laplacian_P1(const FEMProblem& P, double s, double C_scale);

FEMSystem assemble_fractional_integral_laplacian_P1(
    const FEMProblem& P,
    const FractionalIntegralSpec& spec
);

FEMSystem assemble_fractional_regional_laplacian_P1(
    const FEMProblem& P,
    const FractionalRegionalSpec& spec
);

FEMSystem assemble_operator_P1(const FEMProblem& P, const OperatorSpec& op);

FEMSystem assemble_and_solve_operator_P1(
    const FEMProblem& P,
    const OperatorSpec& op,
    DifferentialEquationSolution& out
);

FEMSystem assemble_and_solve_spectral_fractional_P1(
    const FEMProblem& P,
    DifferentialEquationSolution& out
);

struct DirichletMask;

void apply_dirichlet_elimination(
    FEMSystem& S,
    const FEMMesh& M,
    const DirichletMask& D
);

void apply_dirichlet_elimination(
    FEMSystem& S,
    const FEMMesh& M,
    const std::vector<std::tuple<int,double>>& D
);

void solve_linear_system(FEMSystem& sys);

}

#endif
