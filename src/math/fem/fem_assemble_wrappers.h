#ifndef FEM_ASSEMBLE_WRAPPERS
#define FEM_ASSEMBLE_WRAPPERS

#include "fem_assembler.h"
#include "fem_problem.h"

namespace fem {

using AssembleFn = FEMSystem (*)(const FEMProblem&);

inline FEMSystem assemble_local_P1(const FEMProblem& P) {
    return assemble_poisson_P1(P);
}

inline FEMSystem assemble_fractional_integral_dense_P1(const FEMProblem& P) {
    if (const auto* spec = std::get_if<FractionalIntegralSpec>(&P.operator_spec())) {
        return assemble_fractional_integral_laplacian_P1(P, *spec);
    }
    return assemble_fractional_integral_laplacian_P1(P, FractionalIntegralSpec{});
}

inline FEMSystem assemble_fractional_regional_dense_P1(const FEMProblem& P) {
    if (const auto* spec = std::get_if<FractionalRegionalSpec>(&P.operator_spec())) {
        return assemble_fractional_regional_laplacian_P1(P, *spec);
    }
    return assemble_fractional_regional_laplacian_P1(P, FractionalRegionalSpec{});
}

inline FEMSystem assemble_fractional_dense_P1(const FEMProblem& P) {
    return assemble_operator_P1(P, P.operator_spec());
}

inline FEMSystem assemble_auto_P1(const FEMProblem& P) {
    return assemble_operator_P1(P, P.operator_spec());
}

} // namespace fem

#endif
