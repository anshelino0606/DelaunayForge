#include "fractional_integral_operator.h"
#include "math/fem/fem_mesh.h"
#include "math/fem/operators/nodal_mass_builder.h"
#include "math/fem/operators/nonlocal_kernel.h"
#include "math/fem/operators/exterior_interaction_model.h"
#include "math/fem/fem_problem.h"
#include <cmath>

namespace fem {

FractionalElementContribution::FractionalElementContribution(const glm::ivec3& global_vertex_indices)
    : vertex_indices_(global_vertex_indices) {}

static void fill_fractional_element_rhs(
    glm::dvec3& bf,
    const FEMMesh& mesh,
    const FEMProblem& prob,
    const std::vector<double>& nodal_mass,
    const glm::ivec3& vertex_indices
) {
    for (int a = 0; a < 3; ++a) {
        const Index i = to_index_or_invalid(vertex_indices[a]);
        if (!is_valid(i, mesh.nodes.size())) continue;
        const auto& node = mesh.nodes[to_size(i)];
        bf[a] = prob.f(node.x, node.y) * nodal_mass[to_size(i)];
    }
}

void FractionalElementContribution::compute(
    const FEMMesh& mesh, const FractionalIntegralSpec& spec,
    const FEMProblem& prob, const std::vector<double>& nodal_mass
) {
    Af_ = glm::dmat3(0.0);
    bf_ = glm::dvec3(0.0);

    const Index N = mesh.dof_count_index();
    const auto exterior_diag = approximate_integral_exterior_diagonal(
        mesh, nodal_mass, static_cast<double>(spec.s), static_cast<double>(spec.scale));

    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) {
            if (a == b) continue;
            const Index i = to_index_or_invalid(vertex_indices_[a]);
            const Index j = to_index_or_invalid(vertex_indices_[b]);
            if (!is_valid(i, mesh.nodes.size()) || !is_valid(j, mesh.nodes.size())) continue;
            Af_[b][a] = -fractional_pair_weight(
                mesh.nodes[to_size(i)],
                mesh.nodes[to_size(j)],
                nodal_mass[to_size(i)],
                nodal_mass[to_size(j)],
                static_cast<double>(spec.s),
                static_cast<double>(spec.scale)
            );
        }
    }

    for (int a = 0; a < 3; ++a) {
        const Index i = to_index_or_invalid(vertex_indices_[a]);
        if (!is_valid(i, mesh.nodes.size())) continue;
        double diag_accumulation = exterior_diag[to_size(i)];
        for (Index j = 0; j < N; ++j) {
            if (i == j) {
                continue;
            }
            diag_accumulation += fractional_pair_weight(
                mesh.nodes[to_size(i)],
                mesh.nodes[to_size(j)],
                nodal_mass[to_size(i)],
                nodal_mass[to_size(j)],
                static_cast<double>(spec.s),
                static_cast<double>(spec.scale)
            );
        }
        Af_[a][a] = diag_accumulation;
    }

    fill_fractional_element_rhs(bf_, mesh, prob, nodal_mass, vertex_indices_);
}

void FractionalElementContribution::compute(
    const FEMMesh& mesh, const FractionalRegionalSpec& spec,
    const FEMProblem& prob, const std::vector<double>& nodal_mass
) {
    Af_ = glm::dmat3(0.0);
    bf_ = glm::dvec3(0.0);

    const Index N = mesh.dof_count_index();
    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) {
            if (a == b) continue;
            const Index i = to_index_or_invalid(vertex_indices_[a]);
            const Index j = to_index_or_invalid(vertex_indices_[b]);
            if (!is_valid(i, mesh.nodes.size()) || !is_valid(j, mesh.nodes.size())) continue;
            Af_[b][a] = -fractional_pair_weight(
                mesh.nodes[to_size(i)],
                mesh.nodes[to_size(j)],
                nodal_mass[to_size(i)],
                nodal_mass[to_size(j)],
                static_cast<double>(spec.s),
                static_cast<double>(spec.scale)
            );
        }
    }

    for (int a = 0; a < 3; ++a) {
        const Index i = to_index_or_invalid(vertex_indices_[a]);
        if (!is_valid(i, mesh.nodes.size())) continue;
        double diag_accumulation = 0.0;
        for (Index j = 0; j < N; ++j) {
            if (i == j) {
                continue;
            }
            diag_accumulation += fractional_pair_weight(
                mesh.nodes[to_size(i)],
                mesh.nodes[to_size(j)],
                nodal_mass[to_size(i)],
                nodal_mass[to_size(j)],
                static_cast<double>(spec.s),
                static_cast<double>(spec.scale)
            );
        }
        Af_[a][a] = diag_accumulation;
    }

    fill_fractional_element_rhs(bf_, mesh, prob, nodal_mass, vertex_indices_);
}

}