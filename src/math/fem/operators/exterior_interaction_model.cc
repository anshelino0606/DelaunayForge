#include "exterior_interaction_model.h"

#include "math/math_.h"
#include "geom/geom2d/segment.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fem {

std::vector<double> ExteriorInteractionModel::diagonal(
    const FEMMesh& mesh,
    std::span<const double> nodal_mass,
    double s,
    double scale
) const {
    const Index N = mesh.dof_count<Index>();
    std::vector<double> diag(to_size(N), 0.0);
    if (!enabled || N == 0 || mesh.edges_bc.empty()) return diag;

    double mass_length_scale = 0.0;
    for (double mass : nodal_mass) {
        mass_length_scale = std::max(mass_length_scale, std::sqrt(std::max(mass, 0.0)));
    }
    const double min_distance = std::max(1e-6, 0.25 * std::max(1e-6, mass_length_scale));

    for (Index i = 0; i < N; ++i) {
        const FEMMesh::Node& node = mesh.nodes[to_size(i)];
        double boundary_distance = std::numeric_limits<double>::max();

        for (const FEMMesh::EdgeBC& edge : mesh.edges_bc) {
            if (!is_valid(edge.a, mesh.nodes.size()) || !is_valid(edge.b, mesh.nodes.size())) continue;
            const FEMMesh::Node& a = mesh.nodes[to_size(edge.a)];
            const FEMMesh::Node& b = mesh.nodes[to_size(edge.b)];
            boundary_distance = std::min(
                boundary_distance,
                geom2d::segment::point_segment_dist(node, a, b)
            );
        }

        if (boundary_distance == std::numeric_limits<double>::max()) continue;
        const double delta = std::max(boundary_distance, min_distance);
        const double exterior_tail = (math::PI / s) * std::pow(delta, -2.0 * s);
        diag[to_size(i)] = scale * nodal_mass[to_size(i)] * exterior_tail;
    }
    return diag;
}

std::vector<double> approximate_integral_exterior_diagonal(
    const FEMMesh& mesh,
    std::span<const double> nodal_mass,
    double s,
    double scale
) {
    return ExteriorInteractionModel{}.diagonal(mesh, nodal_mass, s, scale);
}

} // namespace fem
