#ifndef FEM_OPERATORS_EXTERIOR_INTERACTION_MODEL_H
#define FEM_OPERATORS_EXTERIOR_INTERACTION_MODEL_H

#include "math/fem/fem_mesh.h"

#include <span>
#include <vector>

namespace fem {

[[nodiscard]] double point_segment_distance(
    double px,
    double py,
    double ax,
    double ay,
    double bx,
    double by
);

struct ExteriorInteractionModel {
    bool enabled = true;

    [[nodiscard]] std::vector<double> diagonal(
        const FEMMesh& mesh,
        std::span<const double> nodal_mass,
        double s,
        double scale
    ) const;
};

[[nodiscard]] std::vector<double> approximate_integral_exterior_diagonal(
    const FEMMesh& mesh,
    std::span<const double> nodal_mass,
    double s,
    double scale
);

} // namespace fem

#endif // FEM_OPERATORS_EXTERIOR_INTERACTION_MODEL_H
