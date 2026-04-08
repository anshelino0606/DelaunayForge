#ifndef FEM_ASSEMBLERS_P1
#define FEM_ASSEMBLERS_P1

#include "fem_solve_pipeline.h"
#include "fem_assemble_wrappers.h"
#include "fem_assembler.h"

namespace fem {

FEMSystem assemble_and_solve_spectral_fractional_P1(const FEMProblem& P, DifferentialEquationSolution& out);


inline FEMSystem assemble_and_solve_local_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_strong_dirichlet(P, &assemble_local_P1, out);
}

inline FEMSystem assemble_and_solve_heat_implicit_euler_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_strong_dirichlet(P, &assemble_heat_implicit_euler_P1, out);
}

inline FEMSystem assemble_and_solve_wave_newmark_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_strong_dirichlet(P, &assemble_wave_newmark_P1, out);
}

inline FEMSystem assemble_and_solve_fractional_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    if (P.fractional && P.fractional->type == FractionalType::Spectral) {
        return assemble_and_solve_spectral_fractional_P1(P, out);
    }
    return assemble_and_solve(P, &assemble_fractional_dense_P1, out);
}

inline FEMSystem assemble_and_solve_auto_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    if (P.fractional && P.fractional->type == FractionalType::Spectral) {
        return assemble_and_solve_spectral_fractional_P1(P, out);
    }
    return assemble_and_solve(P, &assemble_auto_P1, out);
}

inline FEMSystem assemble_and_solve_fractional_auto_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    out.invalidate();
    FEMSystem sys;
    if (!P.mesh || !P.fractional) return sys;

    const auto cfg = *P.fractional;

    switch (cfg.type.value) {
        case FractionalType::Spectral:
            return assemble_and_solve_spectral_fractional_P1(P, out);

        case FractionalType::Integral:
        case FractionalType::Regional:
        default:
            // For now both can share the same dense-kernel builder; later you split.
            return assemble_and_solve(P, &assemble_fractional_dense_P1, out);
    }
}

} // namespace fem

#endif
