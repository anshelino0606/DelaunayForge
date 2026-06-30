#include "fractional_element_contribution.h"
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
    const std::vector<double>&,
    const glm::ivec3& vertex_indices
) {
    Index ids[3];
    for (int a = 0; a < 3; ++a) {
        ids[a] = to_index_or_invalid(vertex_indices[a]);
        if (!is_valid(ids[a], mesh.nodes.size())) {
            return;
        }
    }

    const FEMMesh::Node& A = mesh.nodes[to_size(ids[0])];
    const FEMMesh::Node& B = mesh.nodes[to_size(ids[1])];
    const FEMMesh::Node& C = mesh.nodes[to_size(ids[2])];
    const double area = 0.5 * std::abs((B.x - A.x) * (C.y - A.y) - (C.x - A.x) * (B.y - A.y));
    if (area <= 0.0) {
        return;
    }

    constexpr double l1[3] = {1.0 / 6.0, 2.0 / 3.0, 1.0 / 6.0};
    constexpr double l2[3] = {1.0 / 6.0, 1.0 / 6.0, 2.0 / 3.0};
    constexpr double l3[3] = {2.0 / 3.0, 1.0 / 6.0, 1.0 / 6.0};
    constexpr double w[3] = {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0};

    for (int q = 0; q < 3; ++q) {
        const double x = l1[q] * A.x + l2[q] * B.x + l3[q] * C.x;
        const double y = l1[q] * A.y + l2[q] * B.y + l3[q] * C.y;
        const double N[3] = {l1[q], l2[q], l3[q]};
        const double fq = prob.f(x, y);
        for (int a = 0; a < 3; ++a) {
            bf[a] += w[q] * fq * N[a] * area;
        }
    }
}

void FractionalElementContribution::compute(
    const FEMMesh& mesh, const FractionalIntegralSpec& spec,
    const FEMProblem& prob, const std::vector<double>& nodal_mass
) {
    Af_ = glm::dmat3(0.0);
    bf_ = glm::dvec3(0.0);

    const Index N = mesh.dof_count<Index>();
    const std::vector<double> exterior_diag = approximate_integral_exterior_diagonal(
        mesh, nodal_mass, spec.s, spec.scale);

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
                spec.s,
                spec.scale
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
                spec.s,
                spec.scale
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

    const Index N = mesh.dof_count<Index>();
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
                spec.s,
                spec.scale
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
                spec.s,
                spec.scale
            );
        }
        Af_[a][a] = diag_accumulation;
    }

    fill_fractional_element_rhs(bf_, mesh, prob, nodal_mass, vertex_indices_);
}

}