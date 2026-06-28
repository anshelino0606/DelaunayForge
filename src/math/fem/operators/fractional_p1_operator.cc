#include "fractional_p1_operator.h"

#include "math/fem/fem_boundary_adapter.h"
#include "math/fem/fem_quadrature.h"
#include "math/fem/operators/boundary_load_model.h"
#include "math/fem/operators/exterior_interaction_model.h"
#include "math/fem/operators/nodal_mass_builder.h"
#include "math/fem/operators/nonlocal_kernel.h"

#include <utility>

namespace fem {

void add_symmetric_nonlocal_pair(std::vector<Triplet>& triplets, Index i, Index j, Real weight) {
    if (weight == 0.0) return;
    triplets.push_back({i, j, -weight});
    triplets.push_back({j, i, -weight});
    triplets.push_back({i, i,  weight});
    triplets.push_back({j, j,  weight});
}

void add_consistent_p1_reaction_and_rhs(
    const FEMProblem& problem,
    const FEMMesh& mesh,
    std::vector<Triplet>& triplets,
    std::vector<Real>& rhs
) {
    for (const FEMMesh::Elem& elem : mesh.elems) {
        Real be[3] = {0.0, 0.0, 0.0};
        Real ce[3][3] = {
            {0.0, 0.0, 0.0},
            {0.0, 0.0, 0.0},
            {0.0, 0.0, 0.0}
        };

        for (int q = 0; q < TriQuad3::n; ++q) {
            double x = 0.0;
            double y = 0.0;
            tri_point(mesh, elem, TriQuad3::l1[q], TriQuad3::l2[q], TriQuad3::l3[q], x, y);

            const Real Nq[3] = {
                static_cast<Real>(TriQuad3::l1[q]),
                static_cast<Real>(TriQuad3::l2[q]),
                static_cast<Real>(TriQuad3::l3[q])
            };
            const Real w = static_cast<Real>(TriQuad3::w[q] * elem.area);
            const Real fq = static_cast<Real>(problem.f(x, y));
            const Real cq = static_cast<Real>(problem.c(x, y));

            for (int i = 0; i < 3; ++i) {
                be[i] += w * fq * Nq[i];
                if (cq == Real(0)) continue;
                for (int j = 0; j < 3; ++j) {
                    ce[i][j] += w * cq * Nq[i] * Nq[j];
                }
            }
        }

        for (int i = 0; i < 3; ++i) {
            const Index I = elem.v[i];
            if (!is_valid(I, rhs.size())) continue;
            rhs[to_size(I)] += be[i];
            for (int j = 0; j < 3; ++j) {
                const Index J = elem.v[j];
                if (!is_valid(J, rhs.size())) continue;
                if (ce[i][j] != Real(0)) {
                    triplets.push_back({I, J, ce[i][j]});
                }
            }
        }
    }
}

FEMSystem assemble_fractional_p1_operator_system(
    const FEMProblem& problem,
    const FractionalP1OperatorOptions& options
) {
    FEMSystem system;
    if (!problem.mesh) return system;

    const FEMMesh& mesh = *problem.mesh;
    const Index N = mesh.dof_count<Index>();
    const NonlocalKernel kernel{.s = options.s, .scale = options.scale};

    const std::vector<double> nodal_mass = build_fractional_nodal_mass(mesh);
    std::vector<Real> rhs(to_size(N), Real(0));

    std::vector<Triplet> triplets;
    const std::size_t n_size = to_size(N);
    const std::size_t pair_count = n_size * (n_size > 0 ? n_size - 1u : 0u) / 2u;
    triplets.reserve(4u * pair_count + mesh.elems.size() * 9u + mesh.edges_bc.size() * 4u + n_size);

    for (Index i = 0; i < N; ++i) {
        const FEMMesh::Node& lhs = mesh.nodes[to_size(i)];
        for (Index j = i + 1; j < N; ++j) {
            const FEMMesh::Node& rhs_node = mesh.nodes[to_size(j)];
            const double weight = kernel.pair_weight(
                lhs,
                rhs_node,
                nodal_mass[to_size(i)],
                nodal_mass[to_size(j)]
            );
            add_symmetric_nonlocal_pair(triplets, i, j, static_cast<Real>(weight));
        }
    }

    if (options.include_integral_exterior_tail) {
        const std::vector<double> exterior_diag = ExteriorInteractionModel{}.diagonal(mesh, nodal_mass, options.s, options.scale);
        for (Index i = 0; i < N; ++i) {
            const Real value = static_cast<Real>(exterior_diag[to_size(i)]);
            if (value != Real(0)) {
                triplets.push_back({i, i, value});
            }
        }
    }

    add_consistent_p1_reaction_and_rhs(problem, mesh, triplets, rhs);

    const BoundaryModel boundary = problem.boundary.empty() ? make_boundary_model(mesh) : problem.boundary;
    BoundaryLoadModel{}.add_natural_terms(boundary, mesh, triplets, rhs);

    system.A = build_crs_from_triplets(N, std::move(triplets));
    system.b = std::move(rhs);
    system.x.assign(to_size(N), Real(0));
    return system;
}

} // namespace fem
