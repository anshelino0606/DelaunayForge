#ifndef FEM_ASSEMBLER
#define FEM_ASSEMBLER

#include "fem_problem.h"
#include "fem_element_p1.h"
#include <tuple>
#include "math/differential_equation_solution.h"

namespace fem {

inline constexpr double kDefaultSolveTol   = 1e-8;
inline constexpr int    kDefaultSolveMaxIt = 2000;


template<typename Real = double>
struct FEMSystemT {
    CRS A;
    std::vector<Real> b;
    std::vector<Real> x;
};

using FEMSystem = FEMSystemT<double>;


struct Triplet { int r, c; double v; };

CRS build_crs_from_triplets(int n, std::vector<Triplet> T);

FEMSystem assemble_poisson_P1(const FEMProblem& P);

FEMSystem assemble_heat_implicit_euler_P1(const FEMProblem& P);

FEMSystem assemble_wave_newmark_P1(const FEMProblem& P);

FEMSystem assemble_fractional_laplacian_P1(const FEMProblem& P, double s, double C_scale);

FEMSystem assemble_and_solve_spectral_fractional_P1(
    const FEMProblem& P,
    DifferentialEquationSolution& out
);

void apply_dirichlet_elimination(
    FEMSystem& S,
    const FEMMesh& M,
    const std::vector<std::tuple<int,double>>& D
);

void solve_linear_system(FEMSystem& sys);

}

#endif
