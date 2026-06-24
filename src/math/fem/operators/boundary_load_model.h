#ifndef FEM_OPERATORS_BOUNDARY_LOAD_MODEL_H
#define FEM_OPERATORS_BOUNDARY_LOAD_MODEL_H

#include "math/pde/boundary_model.h"
#include "math/fem/fem_mesh.h"

#include <vector>

namespace fem {

struct BoundaryLoadModel {
    void add_natural_terms(
        const BoundaryModel& boundary,
        const FEMMesh& mesh,
        std::vector<Triplet>& triplets,
        std::vector<Real>& rhs
    ) const;
};

} // namespace fem

#endif // FEM_OPERATORS_BOUNDARY_LOAD_MODEL_H
