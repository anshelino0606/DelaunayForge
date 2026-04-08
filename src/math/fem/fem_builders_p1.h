#ifndef FEM_BUILDERS_P1
#define FEM_BUILDERS_P1

#include "fem_builder.h"
#include "fem_assembler.h"
#include "fem_assembler_generic.h"
#include "fem_integrators.h"

namespace fem {

inline FEMBuilder build_local_p1() {
    return [](const FEMProblem& P) -> FEMSystem {
        return assemble_generic<LocalIntegratorP1<double>, double>(P);
    };
}

inline FEMBuilder build_fractional_p1() {
    return [](const FEMProblem& P) -> FEMSystem {
        if (!P.fractional) return assemble_generic<LocalIntegratorP1<double>, double>(P);
        const auto cfg = *P.fractional;
        return assemble_fractional_laplacian_P1(P, cfg.s, cfg.scale);
    };
}


inline FEMBuilder build_auto_p1() {
    return [](const FEMProblem& P) -> FEMSystem {
        if (P.fractional) {
            const auto cfg = *P.fractional;
            return assemble_fractional_laplacian_P1(P, cfg.s, cfg.scale);
        }
        return assemble_generic<LocalIntegratorP1<double>, double>(P);
    };
}

} // namespace fem


#endif
