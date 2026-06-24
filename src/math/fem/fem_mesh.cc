#include "fem_mesh.h"

namespace fem {

int FEMMesh::dof_count() const {
    return static_cast<int>(nodes.size());
}

Count FEMMesh::dof_count_count() const {
    return to_count(nodes.size());
}

Index FEMMesh::dof_count_index() const {
    return to_index(nodes.size());
}

} // namespace fem
