#ifndef FEM_PROBLEM
#define FEM_PROBLEM

#include "fem_mesh.h"
#include "math/differential_equation.h"
#include "math/pde/operator_spec.h"
#include "math/pde/pde_model.h"
#include "math/pde/solve_request.h"

#include <span>

namespace fem {

struct FEMProblem : public fem::DifferentialEquation {
    const FEMMesh* mesh = nullptr;
    OperatorSpec operator_spec_ = LocalEllipticSpec{};
    BoundaryModel boundary{};
    SolveKind solve_kind = SolveKind::Stationary;

    double dt = 0.0;
    std::span<const double> u_prev;

    FEMProblem() = default;
    explicit FEMProblem(const SolveRequest& request);
    FEMProblem(const SolveRequest& request, const FEMMesh* fem_mesh);

    [[nodiscard]] PDEModel model() const;
    [[nodiscard]] const OperatorSpec& operator_spec() const;
    void set_operator_spec(const OperatorSpec& spec);
    [[nodiscard]] SolveRequest solve_request() const;
};

} // namespace fem

#endif // FEM_PROBLEM
