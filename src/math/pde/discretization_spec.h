#ifndef FEM_PDE_DISCRETIZATION_SPEC_H
#define FEM_PDE_DISCRETIZATION_SPEC_H

#include <cstdint>

namespace fem {

enum class DiscretizationBackend : uint8_t {
    FEM
};

enum class FEMBasisKind : uint8_t {
    P1,
    P2,
    Q1
};

enum class DirichletPolicy : uint8_t {
    StrongElimination,
    StrongSplit
};

struct DiscretizationSpec {
    DiscretizationBackend backend = DiscretizationBackend::FEM;
    FEMBasisKind basis = FEMBasisKind::P1;
    DirichletPolicy dirichlet = DirichletPolicy::StrongElimination;
};

} // namespace fem

#endif // FEM_PDE_DISCRETIZATION_SPEC_H
