#ifndef FEM_MESH
#define FEM_MESH

#include <vector>
#include <functional>
#include "bc_value.h"

namespace fem {

struct CRS {
    std::vector<int>    row_ptr;   // size = n+1
    std::vector<int>    col_idx;   // size = nnz
    std::vector<double> vals;      // size = nnz
};

struct FEMMesh {
    struct Node { double x, y; int id; };
    struct Elem { int v[3]; double area; }; // triangles

    struct EdgeBC {
        int a = -1, b = -1;
        fem::BCType type = fem::BCType::None;

        // Dirichlet: u = uD
        double uD = 0.0;

        // Neumann:   ∂u/∂n = gN   (or flux, depending on your formulation)
        double gN = 0.0;

        // Robin:     ∂u/∂n + k u = g
        double k = 0.0;
        double g = 0.0;
    };

    std::vector<Node>   nodes;
    std::vector<Elem>   elems;
    std::vector<EdgeBC> edges_bc; // only boundary edges with tags

    int dof_count() const { return (int)nodes.size(); }
};

}

#endif
