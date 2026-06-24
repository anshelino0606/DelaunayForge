#ifndef FEM_OPERATORS_FRACTIONAL_P1_OPERATOR_H
#define FEM_OPERATORS_FRACTIONAL_P1_OPERATOR_H

#include "math/fem/fem_boundary_adapter.h"
#include "math/fem/fem_assembler.h"
#include "math/fem/fem_problem.h"
#include "math/operators/boundary_load_model.h"
#include "math/operators/exterior_interaction_model.h"
#include "math/operators/nodal_mass_builder.h"
#include "math/operators/nonlocal_kernel.h"

#include <algorithm>
#include <vector>

namespace fem {

struct FractionalP1OperatorOptions {
    double s = 0.5;
    double scale = 1.0;
    bool include_integral_exterior_tail = false;
};

inline void add_symmetric_nonlocal_pair(std::vector<Triplet>& triplets, Index i, Index j, Real weight) {
    if (weight == 0.0) return;
    triplets.push_back({i, j, -weight});
    triplets.push_back({j, i, -weight});
    triplets.push_back({i, i,  weight});
    triplets.push_back({j, j,  weight});
}

inline FEMSystem assemble_fractional_p1_operator_system(
    const FEMProblem& problem,
    const FractionalP1OperatorOptions& options
) {
    FEMSystem system;
    if (!problem.mesh) return system;

    const FEMMesh& mesh = *problem.mesh;
    const Index N = mesh.dof_count_index();
    const NonlocalKernel kernel{.s = options.s, .scale = options.scale};

    std::vector<double> nodal_mass = build_fractional_nodal_mass(mesh);

    std::vector<double> rhs(to_size(N), 0.0);
    for (Index i = 0; i < N; ++i) {
        const auto& node = mesh.nodes[to_size(i)];
        rhs[to_size(i)] = problem.f(node.x, node.y) * nodal_mass[to_size(i)];
    }

    std::vector<Triplet> triplets;
    const std::size_t pair_count = to_size(N) * (to_size(N) > 0 ? to_size(N) - 1u : 0u) / 2u;
    triplets.reserve(4u * pair_count + (options.include_integral_exterior_tail ? to_size(N) : 0u));

    for (Index i = 0; i < N; ++i) {
        const auto& lhs = mesh.nodes[to_size(i)];
        for (Index j = i + 1; j < N; ++j) {
            const auto& rhs_node = mesh.nodes[to_size(j)];
            const double weight = kernel.pair_weight(
                lhs,
                rhs_node,
                nodal_mass[to_size(i)],
                nodal_mass[to_size(j)]
            );
            add_symmetric_nonlocal_pair(triplets, i, j, weight);
        }
    }

    if (options.include_integral_exterior_tail) {
        const auto exterior_diag = ExteriorInteractionModel{}.diagonal(mesh, nodal_mass, options.s, options.scale);
        for (Index i = 0; i < N; ++i) {
            triplets.push_back({i, i, exterior_diag[to_size(i)]});
        }
    }

    const BoundaryModel boundary = problem.boundary.empty() ? make_boundary_model(mesh) : problem.boundary;
    BoundaryLoadModel{}.add_natural_terms(boundary, mesh, triplets, rhs);

    system.A = build_crs_from_triplets(N, std::move(triplets));
    system.b = std::move(rhs);
    system.x.assign(to_size(N), 0.0);
    return system;
}

} // namespace fem

#endif // FEM_OPERATORS_FRACTIONAL_P1_OPERATOR_H
