#include "fem_problem.h"

namespace fem {

FEMProblem::FEMProblem(const SolveRequest& request)
    : fem::DifferentialEquation()
{
    request.model.apply_to(*this);
    operator_spec_ = request.operator_spec;
    boundary = request.boundary;
    solve_kind = request.solve_kind;
    dt = request.time_step.dt;
    u_prev = request.time_step.previous_state;
}

FEMProblem::FEMProblem(const SolveRequest& request, const FEMMesh* fem_mesh)
    : FEMProblem(request)
{
    mesh = fem_mesh;
}

PDEModel FEMProblem::model() const {
    return PDEModel(*this);
}

const OperatorSpec& FEMProblem::operator_spec() const {
    return operator_spec_;
}

void FEMProblem::set_operator_spec(const OperatorSpec& spec) {
    operator_spec_ = spec;
}

SolveRequest FEMProblem::solve_request() const {
    return SolveRequest{
        .model = model(),
        .operator_spec = operator_spec_,
        .discretization = {},
        .boundary = boundary,
        .solve_kind = solve_kind,
        .time_step = TimeStepState{.dt = dt, .previous_state = u_prev}
    };
}

} // namespace fem
