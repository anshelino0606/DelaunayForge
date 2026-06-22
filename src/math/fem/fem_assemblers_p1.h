#ifndef FEM_ASSEMBLERS_P1
#define FEM_ASSEMBLERS_P1

#include "fem_solve_pipeline.h"
#include "fem_assemble_wrappers.h"
#include "fem_assembler.h"
#include "fem_operator_dispatch.h"

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
    return assemble_and_solve_operator_P1(P, P.operator_spec(), out);
}

inline FEMSystem assemble_and_solve_auto_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_operator_P1(P, P.operator_spec(), out);
}

inline FEMSystem assemble_and_solve_fractional_auto_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    out.invalidate();
    FEMSystem sys;
    if (!P.mesh || std::holds_alternative<LocalEllipticSpec>(P.operator_spec())) return sys;
    return assemble_and_solve_operator_P1(P, P.operator_spec(), out);
}

} // namespace fem

#endif
