#ifndef FEM_ASSEMBLE_WRAPPERS
#define FEM_ASSEMBLE_WRAPPERS

#include "fem_assembler.h"
#include "fem_problem.h"

namespace fem {

using AssembleFn = FEMSystem (*)(const FEMProblem&);

inline FEMSystem assemble_local_P1(const FEMProblem& P) {
    return assemble_poisson_P1(P);
}

inline FEMSystem assemble_fractional_dense_P1(const FEMProblem& P) {
    if (!P.fractional) return assemble_poisson_P1(P);
    const auto cfg = *P.fractional;
    return assemble_fractional_laplacian_P1(P, (double)cfg.s, (double)cfg.scale);
}


inline FEMSystem assemble_auto_P1(const FEMProblem& P) {
    if (!P.fractional) return assemble_local_P1(P);
    const auto cfg = *P.fractional;

    if (cfg.type == FractionalType::Spectral) return assemble_local_P1(P);
    return assemble_fractional_dense_P1(P);
}


} // namespace fem

#endif
