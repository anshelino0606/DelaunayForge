#ifndef FEM_MESH
#define FEM_MESH

#include <array>
#include <vector>
#include "bc_value.h"
#include "math/types.h"
#include <glm/vec2.hpp>

namespace fem {

struct CRS {
    std::vector<Index> row_ptr;
    std::vector<Index> col_idx;
    std::vector<Real> vals;
};

struct Triplet {
    Index r = invalid_index;
    Index c = invalid_index;
    Real v = 0.0;
};

struct FEMMesh {
    struct Node : public glm::vec<2, Real, glm::defaultp> {
        Node() {
            x = 0.0;
            y = 0.0;
        }

        Node(Real in_x, Real in_y, Index in_id) {
            x = in_x, y = in_y, id = in_id;
        }

        Index id = invalid_index;
    };

    struct Elem {
        std::array<Index, 3> v{};
        Real area = 0.0;
    };

    struct EdgeBC {
        Index a = invalid_index;
        Index b = invalid_index;
        fem::BCType type = fem::BCType::None;
        Real uD = 0.0;
        Real gN = 0.0;
        Real k = 0.0;
        Real g = 0.0;
    };

    std::vector<Node> nodes;
    std::vector<Elem> elems;
    std::vector<EdgeBC> edges_bc;

    [[nodiscard]] int dof_count() const;
    [[nodiscard]] Count dof_count_count() const;
    [[nodiscard]] Index dof_count_index() const;
};

} // namespace fem

#endif
