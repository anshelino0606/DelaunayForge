#ifndef FEM_MESH
#define FEM_MESH

#include <array>
#include <vector>
#include <functional>
#include "bc_value.h"
#include "math/types.h"

namespace fem {

struct CRS {
    std::vector<Index>  row_ptr;   // size = n+1
    std::vector<Index>  col_idx;   // size = nnz
    std::vector<Real>   vals;      // size = nnz
};

struct Triplet {
    Index r = invalid_index;
    Index c = invalid_index;
    Real v = 0.0;
};

struct FEMMesh {
    struct Node {
        Real x = 0.0;
        Real y = 0.0;
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

        // Dirichlet: u = uD
        Real uD = 0.0;

        // Neumann:   ∂u/∂n = gN   (or flux, depending on your formulation)
        Real gN = 0.0;

        // Robin:     ∂u/∂n + k u = g
        Real k = 0.0;
        Real g = 0.0;
    };

    std::vector<Node>   nodes;
    std::vector<Elem>   elems;
    std::vector<EdgeBC> edges_bc; // only boundary edges with tags

    [[nodiscard]] int dof_count() const { return static_cast<int>(nodes.size()); }
    [[nodiscard]] Count dof_count_count() const { return to_count(nodes.size()); }
    [[nodiscard]] Index dof_count_index() const { return to_index(nodes.size()); }
};

}

#endif
