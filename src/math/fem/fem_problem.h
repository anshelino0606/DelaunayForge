#ifndef FEM_PROBLEM
#define FEM_PROBLEM

#include "fem_mesh.h"
#include "math/differential_equation.h"
#include "math/pde/operator_spec.h"
#include "math/pde/pde_model.h"
#include "math/pde/solve_request.h"
#include <functional>
#include <vector>
#include <span>

namespace fem {

// FEMProblem is now a backend adapter.  Public fields are kept for legacy callers.
struct FEMProblem : public fem::DifferentialEquation {
    const FEMMesh* mesh = nullptr;
    OperatorSpec operator_spec_ = LocalEllipticSpec{};
    BoundaryModel boundary{};
    SolveKind solve_kind = SolveKind::Stationary;

    double dt = 0.0;
    std::span<const double> u_prev;

    FEMProblem() = default;

    explicit FEMProblem(const SolveRequest& request)
        : fem::DifferentialEquation() {
        request.model.apply_to(*this);
        operator_spec_ = request.operator_spec;
        boundary = request.boundary;
        solve_kind = request.solve_kind;
        dt = request.time_step.dt;
        u_prev = request.time_step.previous_state;
    }

    FEMProblem(const SolveRequest& request, const FEMMesh* fem_mesh)
        : FEMProblem(request) {
        mesh = fem_mesh;
    }

    [[nodiscard]] PDEModel model() const { return PDEModel(*this); }

    [[nodiscard]] const OperatorSpec& operator_spec() const {
        return operator_spec_;
    }

    void set_operator_spec(const OperatorSpec& spec) {
        operator_spec_ = spec;
    }

    [[nodiscard]] SolveRequest solve_request() const {
        return SolveRequest{
            .model = model(),
            .operator_spec = operator_spec_,
            .discretization = {},
            .boundary = boundary,
            .solve_kind = solve_kind,
            .time_step = TimeStepState{.dt = dt, .previous_state = u_prev}
        };
    }
};

} // namespace fem

#endif // FEM_PROBLEM
