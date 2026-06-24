#ifndef FEM_OPERATORS_FRACTIONAL_P1_OPERATOR_H
#define FEM_OPERATORS_FRACTIONAL_P1_OPERATOR_H

#include "math/fem/fem_assembler.h"
#include "math/fem/fem_problem.h"

#include <vector>

namespace fem {

struct FractionalP1OperatorOptions {
    double s = 0.5;
    double scale = 1.0;
    bool include_integral_exterior_tail = false;
};

void add_symmetric_nonlocal_pair(std::vector<Triplet>& triplets, Index i, Index j, Real weight);
void add_consistent_p1_reaction_and_rhs(
    const FEMProblem& problem,
    const FEMMesh& mesh,
    std::vector<Triplet>& triplets,
    std::vector<Real>& rhs
);

[[nodiscard]] FEMSystem assemble_fractional_p1_operator_system(
    const FEMProblem& problem,
    const FractionalP1OperatorOptions& options
);

} // namespace fem

#endif // FEM_OPERATORS_FRACTIONAL_P1_OPERATOR_H
