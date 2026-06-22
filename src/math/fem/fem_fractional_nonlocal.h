#pragma once

#include "fem_mesh.h"
#include "math/math_.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace fem {

inline std::vector<double> build_fractional_nodal_mass(const FEMMesh& mesh) {
    std::vector<double> nodal_mass(mesh.dof_count(), 0.0);
    for (const auto& elem : mesh.elems) {
        const double share = elem.area / 3.0;
        nodal_mass[elem.v[0]] += share;
        nodal_mass[elem.v[1]] += share;
        nodal_mass[elem.v[2]] += share;
    }
    return nodal_mass;
}

inline double fractional_pair_weight(
    const FEMMesh::Node& lhs,
    const FEMMesh::Node& rhs,
    double lhs_mass,
    double rhs_mass,
    double s,
    double scale
) {
    const double dx = lhs.x - rhs.x;
    const double dy = lhs.y - rhs.y;
    const double r2 = dx * dx + dy * dy;
    if (r2 == 0.0) {
        return 0.0;
    }

    const double s_exp = 1.0 + s;
    return scale * lhs_mass * rhs_mass / std::pow(r2, s_exp);
}

inline double point_segment_distance(
    double px, double py,
    double ax, double ay,
    double bx, double by
) {
    const double abx = bx - ax;
    const double aby = by - ay;
    const double apx = px - ax;
    const double apy = py - ay;
    const double denom = abx * abx + aby * aby;

    if (denom <= 0.0) {
        const double dx = px - ax;
        const double dy = py - ay;
        return std::hypot(dx, dy);
    }

    const double t = std::clamp((apx * abx + apy * aby) / denom, 0.0, 1.0);
    const double qx = ax + t * abx;
    const double qy = ay + t * aby;
    return std::hypot(px - qx, py - qy);
}

inline std::vector<double> approximate_integral_exterior_diagonal(
    const FEMMesh& mesh,
    std::span<const double> nodal_mass,
    double s,
    double scale
) {
    const int N = mesh.dof_count();
    std::vector<double> diag(static_cast<size_t>(N), 0.0);
    if (N == 0 || mesh.edges_bc.empty()) {
        return diag;
    }

    double mass_length_scale = 0.0;
    for (double mass : nodal_mass) {
        mass_length_scale = std::max(mass_length_scale, std::sqrt(std::max(mass, 0.0)));
    }
    const double min_distance = std::max(1e-6, 0.25 * std::max(1e-6, mass_length_scale));

    for (int i = 0; i < N; ++i) {
        const auto& node = mesh.nodes[static_cast<size_t>(i)];
        double boundary_distance = std::numeric_limits<double>::infinity();

        for (const auto& edge : mesh.edges_bc) {
            if (edge.a < 0 || edge.b < 0) {
                continue;
            }

            const auto& a = mesh.nodes[static_cast<size_t>(edge.a)];
            const auto& b = mesh.nodes[static_cast<size_t>(edge.b)];
            boundary_distance = std::min(
                boundary_distance,
                point_segment_distance(node.x, node.y, a.x, a.y, b.x, b.y)
            );
        }

        if (!std::isfinite(boundary_distance)) {
            continue;
        }

        const double delta = std::max(boundary_distance, min_distance);
        const double exterior_tail = (Math::pi / s) * std::pow(delta, -2.0 * s);
        diag[static_cast<size_t>(i)] = scale * nodal_mass[static_cast<size_t>(i)] * exterior_tail;
    }

    return diag;
}

} // namespace fem