// fem_assembler.cpp
#include "fem_assembler.h"
#include <algorithm>
#include <cmath>
#include <tuple>
#include "fem_problem.h"
#include "math/differential_equation_solution.h"
#include "fem_fractional_spectral_p1.h"
#include "fem_assembler_generic.h"
#include "fem_integrators.h"
#include "math/fem/fem_boundary_adapter.h"
#include "math/fem/operators/fractional_p1_operator.h"
#include "fem_solver_cg.h"

namespace fem {

CRS build_crs_from_triplets(int n, std::vector<Triplet> T) {
    return build_crs_from_triplets(n < 0 ? Index{0} : static_cast<Index>(n), std::move(T));
}

CRS build_crs_from_triplets(Index n, std::vector<Triplet> T) {
    std::sort(T.begin(), T.end(), [](const Triplet& a, const Triplet& b){
        return std::tie(a.r, a.c) < std::tie(b.r, b.c);
    });

    std::vector<Index> row_ptr(to_size(n) + 1u, 0);
    std::vector<Index> col_idx;
    std::vector<Real> vals;
    col_idx.reserve(T.size());
    vals.reserve(T.size());

    Index cur_row = 0;
    Index last_r = invalid_index, last_c = invalid_index;

    for (const Triplet& t : T) {
        while (cur_row < t.r) {
            row_ptr[to_size(++cur_row)] = to_index(col_idx.size());
            last_r = invalid_index; last_c = invalid_index;
        }
        if (t.r == last_r && t.c == last_c) vals.back() += t.v;
        else {
            col_idx.push_back(t.c);
            vals.push_back(t.v);
            last_r = t.r; last_c = t.c;
        }
    }
    while (cur_row < n) row_ptr[to_size(++cur_row)] = to_index(col_idx.size());
    return CRS{ std::move(row_ptr), std::move(col_idx), std::move(vals) };
}


void fill_solution(const FEMSystem& sys, DifferentialEquationSolution& out) {
    out.solution_u = sys.x;
    if (out.solution_u.empty()) {
        out.u_min = 0.0;
        out.u_max = 0.0;
        return;
    }

    const std::pair<std::vector<double>::iterator, std::vector<double>::iterator> bounds =
        std::minmax_element(out.solution_u.begin(), out.solution_u.end());
    out.u_min = *bounds.first;
    out.u_max = *bounds.second;
}


FEMSystem assemble_poisson_P1(const FEMProblem& P) {
    return assemble_generic<LocalIntegratorP1<double>, double>(P);
}

FEMSystem assemble_heat_implicit_euler_P1(const FEMProblem& P) {
    return assemble_generic<HeatImplicitEulerIntegratorP1<double>, double>(P);
}

FEMSystem assemble_wave_newmark_P1(const FEMProblem& P) {
    return assemble_generic<WaveNewmarkIntegratorP1<double>, double>(P);
}



void apply_dirichlet_elimination(FEMSystem& S, const FEMMesh&,
                                 const DirichletMask& D)
{
    const Index N = to_index(S.b.size());
    if (D.is_dirichlet.size() != S.b.size()) [[unlikely]] return;

    std::vector<Triplet> rebuilt;
    rebuilt.reserve(S.A.vals.size() + S.b.size());

    for (Index r = 0; r < N; ++r) {
        if (D.contains(r)) {
            rebuilt.push_back({r, r, 1.0});
            S.b[to_size(r)] = D.value[to_size(r)];
            continue;
        }

        for (Index k = S.A.row_ptr[to_size(r)]; k < S.A.row_ptr[to_size(r + 1)]; ++k) {
            const Index c = S.A.col_idx[to_size(k)];
            const Real v = S.A.vals[to_size(k)];
            if (D.contains(c)) {
                S.b[to_size(r)] -= v * D.value[to_size(c)];
                continue;
            }
            rebuilt.push_back({r, c, v});
        }
    }

    S.A = build_crs_from_triplets(N, std::move(rebuilt));
    if (S.x.size() != S.b.size()) {
        S.x.assign(S.b.size(), 0.0);
    }
    for (Index i = 0; i < N; ++i) {
        if (D.contains(i)) {
            S.x[to_size(i)] = D.value[to_size(i)];
        }
    }
}


FEMSystem assemble_fractional_laplacian_P1(const FEMProblem& P,
                                           double s,
                                           double C_scale)
{
    return assemble_fractional_integral_laplacian_P1(P, FractionalIntegralSpec{s, C_scale});
}

FEMSystem assemble_fractional_integral_laplacian_P1(
    const FEMProblem& P,
    const FractionalIntegralSpec& spec
) {
    return assemble_fractional_p1_operator_system(P, FractionalP1OperatorOptions{.s = static_cast<double>(spec.s), .scale = static_cast<double>(spec.scale), .include_integral_exterior_tail = true});
}

FEMSystem assemble_fractional_regional_laplacian_P1(
    const FEMProblem& P,
    const FractionalRegionalSpec& spec
) {
    return assemble_fractional_p1_operator_system(P, FractionalP1OperatorOptions{.s = static_cast<double>(spec.s), .scale = static_cast<double>(spec.scale), .include_integral_exterior_tail = false});
}

namespace {

struct AssembleP1OperatorVisitor {
    const FEMProblem& problem;

    FEMSystem operator()(const LocalEllipticSpec&) const {
        return assemble_poisson_P1(problem);
    }

    FEMSystem operator()(const FractionalIntegralSpec& spec) const {
        return assemble_fractional_integral_laplacian_P1(problem, spec);
    }

    FEMSystem operator()(const FractionalRegionalSpec& spec) const {
        return assemble_fractional_regional_laplacian_P1(problem, spec);
    }

    FEMSystem operator()(const FractionalSpectralSpec&) const {
        return {};
    }
};

} // namespace

FEMSystem assemble_operator_P1(const FEMProblem& P, const OperatorSpec& op) {
    return std::visit(AssembleP1OperatorVisitor{P}, op);
}

FEMSystem assemble_and_solve_operator_P1(
    const FEMProblem& P,
    const OperatorSpec& op,
    DifferentialEquationSolution& out
) {
    FEMProblem local = P;
    local.set_operator_spec(op);

    if (std::holds_alternative<FractionalSpectralSpec>(op)) {
        return assemble_and_solve_spectral_fractional_P1(local, out);
    }

    out.invalidate();
    FEMSystem sys;
    if (!local.mesh) return sys;

    sys = assemble_operator_P1(local, op);
    const BoundaryModel boundary = local.boundary.empty() ? make_boundary_model(*local.mesh) : local.boundary;
    apply_dirichlet_elimination(sys, *local.mesh, build_dirichlet_mask(boundary, local.mesh->dof_count_count()));
    solve_linear_system(sys);
    fill_solution(sys, out);
    return sys;
}

FEMSystem assemble_and_solve_spectral_fractional_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_fractional_spectral_P1(P, out);
}

static FEMSystem assemble_and_solve_with(
    const FEMProblem& P,
    FEMSystem (*assemble)(const FEMProblem&),
    DifferentialEquationSolution& out
) {
    out.invalidate();
    FEMSystem sys;
    if (!P.mesh || !assemble) {
        return sys;
    }

    sys = assemble(P);
    solve_linear_system(sys);
    fill_solution(sys, out);
    return sys;
}

FEMSystem assemble_and_solve_local_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_with(P, &assemble_poisson_P1, out);
}

FEMSystem assemble_and_solve_heat_implicit_euler_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_with(P, &assemble_heat_implicit_euler_P1, out);
}

FEMSystem assemble_and_solve_wave_newmark_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_with(P, &assemble_wave_newmark_P1, out);
}

FEMSystem assemble_and_solve_fractional_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_operator_P1(P, P.operator_spec(), out);
}

void solve_linear_system(FEMSystem& sys) {
    FEMSolverCG cg;
    cg.tol    = kDefaultSolveTol;
    cg.max_it = kDefaultSolveMaxIt;
    cg.solve(sys.A, sys.b, sys.x);
}

}
