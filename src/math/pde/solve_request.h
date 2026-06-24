#ifndef FEM_PDE_SOLVE_REQUEST_H
#define FEM_PDE_SOLVE_REQUEST_H

#include "math/pde/pde_model.h"
#include "math/pde/operator_spec.h"
#include "math/pde/discretization_spec.h"
#include "math/pde/boundary_model.h"

#include <span>

namespace fem {

enum class SolveKind : uint8_t {
    Stationary,
    HeatImplicitEuler,
    WaveNewmark
};

struct TimeStepState {
    double dt = 0.0;
    std::span<const double> previous_state{};
};

struct SolveRequest {
    PDEModel model;
    OperatorSpec operator_spec = LocalEllipticSpec{};
    DiscretizationSpec discretization{};
    BoundaryModel boundary{};
    SolveKind solve_kind = SolveKind::Stationary;
    TimeStepState time_step{};
};

[[nodiscard]] bool is_transient_solve(SolveKind kind) noexcept;

} // namespace fem

#endif // FEM_PDE_SOLVE_REQUEST_H
