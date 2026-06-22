#ifndef FEM_PDE_SOLVE_REQUEST_H
#define FEM_PDE_SOLVE_REQUEST_H

#include "pde_model.h"
#include "operator_spec.h"
#include "discretization_spec.h"

namespace fem {

struct SolveRequest {
    PDEModel model;
    OperatorSpec operator_spec = LocalEllipticSpec{};
    DiscretizationSpec discretization{};
};

} // namespace fem

#endif // FEM_PDE_SOLVE_REQUEST_H
