#ifndef FEM_OPERATOR
#define FEM_OPERATOR

#include <functional>
#include "fem_problem.h"
#include "fem_assembler.h"

namespace fem {

template<class Real>
struct LocalEllipticP1 {
    FEMSystem build(const FEMProblem& P) const { return assemble_poisson_P1(P); }
};

template<class Real>
struct FractionalDenseP1 {
    FractionalEquationConfig cfg;
    FEMSystem build(const FEMProblem& P) const {
        return assemble_fractional_laplacian_P1(P, (double)cfg.s, (double)cfg.scale);
    }
};

template<class Real>
using SpatialOperator = std::variant<LocalEllipticP1<Real>, FractionalDenseP1<Real>>;

template<class Real>
inline FEMSystem build_system(const FEMProblem& P, const SpatialOperator<Real>& op) {
    return std::visit([&](auto const& o) { return o.build(P); }, op);
}

}

#endif
