#include "boundary_load_model.h"
#include "geom/geom2d/vec.h"

#include <cmath>

namespace fem {

void BoundaryLoadModel::add_natural_terms(
    const BoundaryModel& boundary,
    const FEMMesh& mesh,
    std::vector<Triplet>& triplets,
    std::vector<Real>& rhs
) const {
    for (const BoundaryFace& face : boundary.faces) {
        if (face.kind == BCType::None || face.kind == BCType::Dirichlet) continue;
        if (!is_valid(face.a, mesh.nodes.size()) || !is_valid(face.b, mesh.nodes.size())) continue;

        const FEMMesh::Node& A = mesh.nodes[to_size(face.a)];
        const FEMMesh::Node& B = mesh.nodes[to_size(face.b)];
        const Real L = static_cast<Real>(geom2d::vec::dist(B, A));
        if (L <= Real(0)) continue;

        if (face.kind == BCType::Neumann) {
            const Real gN = static_cast<Real>(face.gN);
            rhs[to_size(face.a)] += gN * L * Real(0.5);
            rhs[to_size(face.b)] += gN * L * Real(0.5);
            continue;
        }

        if (face.kind == BCType::Robin) {
            const Real k = static_cast<Real>(face.k);
            const Real g = static_cast<Real>(face.g);

            const Real m00 = L * Real(2.0 / 6.0);
            const Real m01 = L * Real(1.0 / 6.0);
            const Real m11 = L * Real(2.0 / 6.0);

            triplets.push_back({face.a, face.a, k * m00});
            triplets.push_back({face.a, face.b, k * m01});
            triplets.push_back({face.b, face.a, k * m01});
            triplets.push_back({face.b, face.b, k * m11});

            rhs[to_size(face.a)] += g * L * Real(0.5);
            rhs[to_size(face.b)] += g * L * Real(0.5);
        }
    }
}

} // namespace fem
