#ifndef FEM_OPERATORS_BOUNDARY_LOAD_MODEL_H
#define FEM_OPERATORS_BOUNDARY_LOAD_MODEL_H

#include "math/pde/boundary_model.h"
#include "math/fem/fem_mesh.h"
#include "math/types.h"

#include <cmath>
#include <vector>

namespace fem {

struct BoundaryLoadModel {
    template<typename Real>
    void add_natural_terms(
        const BoundaryModel& boundary,
        const FEMMesh& mesh,
        std::vector<Triplet>& triplets,
        std::vector<Real>& rhs
    ) const {
        for (const BoundaryFace& face : boundary.faces) {
            if (face.kind == BoundaryKind::None || face.kind == BoundaryKind::Dirichlet) continue;
            if (!is_valid(face.a, mesh.nodes.size()) || !is_valid(face.b, mesh.nodes.size())) continue;

            const auto& A = mesh.nodes[to_size(face.a)];
            const auto& B = mesh.nodes[to_size(face.b)];
            const Real L = static_cast<Real>(std::hypot(B.x - A.x, B.y - A.y));
            if (L <= Real(0)) continue;

            if (face.kind == BoundaryKind::Neumann) {
                const Real gN = static_cast<Real>(face.gN);
                rhs[to_size(face.a)] += gN * L * Real(0.5);
                rhs[to_size(face.b)] += gN * L * Real(0.5);
                continue;
            }

            if (face.kind == BoundaryKind::Robin) {
                const Real k = static_cast<Real>(face.k);
                const Real g = static_cast<Real>(face.g);

                const Real m00 = L * Real(2.0 / 6.0);
                const Real m01 = L * Real(1.0 / 6.0);
                const Real m11 = L * Real(2.0 / 6.0);

                triplets.push_back({face.a, face.a, static_cast<fem::Real>(k * m00)});
                triplets.push_back({face.a, face.b, static_cast<fem::Real>(k * m01)});
                triplets.push_back({face.b, face.a, static_cast<fem::Real>(k * m01)});
                triplets.push_back({face.b, face.b, static_cast<fem::Real>(k * m11)});

                rhs[to_size(face.a)] += g * L * Real(0.5);
                rhs[to_size(face.b)] += g * L * Real(0.5);
            }
        }
    }
};

} // namespace fem

#endif // FEM_OPERATORS_BOUNDARY_LOAD_MODEL_H
