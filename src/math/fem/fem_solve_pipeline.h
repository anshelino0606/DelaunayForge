#ifndef FEM_SOLVE_PIPELINE
#define FEM_SOLVE_PIPELINE

#include <tuple>
#include <vector>
#include <algorithm>
#include <exception>
#include "fem_builder.h"
#include "fem_solver_cg.h"
#include "math/differential_equation_solution.h"
#include "fem_assembler.h"
#include "fem_assemble_wrappers.h"

namespace fem {

using FEMAssembler = FEMSystem (*)(const FEMProblem&, DifferentialEquationSolution&);

inline std::vector<std::tuple<int,double>>
gather_dirichlet_set(const FEMMesh& mesh) {
    std::vector<int>    dcnt(mesh.dof_count(), 0);
    std::vector<double> dsum(mesh.dof_count(), 0.0);

    for (const auto& e : mesh.edges_bc) {
        if (e.type == fem::BCType::Dirichlet) {
            dcnt[e.a]++; dsum[e.a] += e.uD;
            dcnt[e.b]++; dsum[e.b] += e.uD;
        }
    }

    std::vector<std::tuple<int,double>> dirichlet;
    dirichlet.reserve(mesh.dof_count());
    for (int i = 0; i < mesh.dof_count(); ++i) {
        if (dcnt[i]) {
            double val = dsum[i] / std::max(1, dcnt[i]);
            dirichlet.emplace_back(i, val);
        }
    }
    return dirichlet;
}

inline void fill_solution(const FEMSystem& sys, DifferentialEquationSolution& out) {
    out.solution_u = sys.x;
    if (out.solution_u.empty()) { out.u_min = out.u_max = 0.0; return; }
    auto [mn, mx] = std::minmax_element(out.solution_u.begin(), out.solution_u.end());
    out.u_min = *mn;
    out.u_max = *mx;
}

inline FEMSystem assemble_and_solve_strong_dirichlet(
    const FEMProblem& P,
    AssembleFn assemble,
    DifferentialEquationSolution& out
) {
    out.invalidate();
    FEMSystem sys;
    if (!P.mesh || !assemble) return sys;

    sys = assemble(P);

    solve_linear_system(sys);
    fill_solution(sys, out);
    return sys;
}

inline std::vector<std::tuple<int,double>>
extract_dirichlet_set(const FEMMesh& mesh) {
    std::vector<int>    dcnt(mesh.dof_count(), 0);
    std::vector<double> dsum(mesh.dof_count(), 0.0);

    for (const auto& e : mesh.edges_bc) {
        if (e.type == fem::BCType::Dirichlet) {
            dcnt[e.a]++; dsum[e.a] += e.uD;
            dcnt[e.b]++; dsum[e.b] += e.uD;
        }
    }

    std::vector<std::tuple<int,double>> D;
    D.reserve(mesh.dof_count());
    for (int i = 0; i < mesh.dof_count(); ++i) {
        if (dcnt[i]) D.emplace_back(i, dsum[i] / std::max(1, dcnt[i]));
    }
    return D;
}

// weak legacy variation - need to deprecate
inline FEMSystem assemble_and_solve(
    const FEMProblem& P,
    AssembleFn assemble,
    DifferentialEquationSolution& out
) {
    out.invalidate();
    FEMSystem sys;
    if (!P.mesh || !assemble) return sys;

    sys = assemble(P);

    auto D = gather_dirichlet_set(*P.mesh);
    apply_dirichlet_elimination(sys, *P.mesh, D);

    solve_linear_system(sys);
    fill_solution(sys, out);
    return sys;
}

} // namespace fem

#endif
