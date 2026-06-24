#include "fractional_integral_operator.h"
#include "fem/fem_mesh.h"
#include "operators/nodal_mass_builder.h"
#include "operators/nonlocal_kernel.h"
#include "operators/exterior_interaction_model.h"
#include "fem/fem_problem.h"
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
        const int i = vertex_indices[a];
        const auto& node = mesh.nodes[static_cast<size_t>(i)];
        bf[a] = prob.f(node.x, node.y) * nodal_mass[static_cast<size_t>(i)];
    }
}

void FractionalElementContribution::compute(
    const FEMMesh& mesh, const FractionalIntegralSpec& spec,
    const FEMProblem& prob, const std::vector<double>& nodal_mass
) {
    Af_ = glm::dmat3(0.0);
    bf_ = glm::dvec3(0.0);

    const int N = mesh.dof_count();
    const auto exterior_diag = approximate_integral_exterior_diagonal(
        mesh, nodal_mass, static_cast<double>(spec.s), static_cast<double>(spec.scale));

    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) {
            if (a == b) continue;
            const int i = vertex_indices_[a];
            const int j = vertex_indices_[b];
            Af_[b][a] = -fractional_pair_weight(
                mesh.nodes[static_cast<size_t>(i)],
                mesh.nodes[static_cast<size_t>(j)],
                nodal_mass[static_cast<size_t>(i)],
                nodal_mass[static_cast<size_t>(j)],
                static_cast<double>(spec.s),
                static_cast<double>(spec.scale)
            );
        }
    }

    for (int a = 0; a < 3; ++a) {
        const int i = vertex_indices_[a];
        double diag_accumulation = exterior_diag[static_cast<size_t>(i)];
        for (int j = 0; j < N; ++j) {
            if (i == j) {
                continue;
            }
            diag_accumulation += fractional_pair_weight(
                mesh.nodes[static_cast<size_t>(i)],
                mesh.nodes[static_cast<size_t>(j)],
                nodal_mass[static_cast<size_t>(i)],
                nodal_mass[static_cast<size_t>(j)],
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

    const int N = mesh.dof_count();
    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) {
            if (a == b) continue;
            const int i = vertex_indices_[a];
            const int j = vertex_indices_[b];
            Af_[b][a] = -fractional_pair_weight(
                mesh.nodes[static_cast<size_t>(i)],
                mesh.nodes[static_cast<size_t>(j)],
                nodal_mass[static_cast<size_t>(i)],
                nodal_mass[static_cast<size_t>(j)],
                static_cast<double>(spec.s),
                static_cast<double>(spec.scale)
            );
        }
    }

    for (int a = 0; a < 3; ++a) {
        const int i = vertex_indices_[a];
        double diag_accumulation = 0.0;
        for (int j = 0; j < N; ++j) {
            if (i == j) {
                continue;
            }
            diag_accumulation += fractional_pair_weight(
                mesh.nodes[static_cast<size_t>(i)],
                mesh.nodes[static_cast<size_t>(j)],
                nodal_mass[static_cast<size_t>(i)],
                nodal_mass[static_cast<size_t>(j)],
                static_cast<double>(spec.s),
                static_cast<double>(spec.scale)
            );
        }
        Af_[a][a] = diag_accumulation;
    }

    fill_fractional_element_rhs(bf_, mesh, prob, nodal_mass, vertex_indices_);
}

}