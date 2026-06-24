#include "fem_boundary_adapter.h"

#include <vector>

namespace fem {

namespace {

void add_dirichlet_node(
    Index node,
    Real value,
    const DirichletMask& mask,
    std::vector<Count>& count,
    std::vector<Real>& sum
) {
    if (!is_valid(node, mask.is_dirichlet.size())) {
        return;
    }
    count[to_size(node)] += Count{1};
    sum[to_size(node)] += value;
}

} // namespace


BoundaryModel make_boundary_model(const FEMMesh& mesh) {
    BoundaryModel model;
    model.faces.reserve(mesh.edges_bc.size());
    for (const FEMMesh::EdgeBC& edge : mesh.edges_bc) {
        if (!is_valid(edge.a, mesh.nodes.size()) || !is_valid(edge.b, mesh.nodes.size())) {
            continue;
        }

        BoundaryFace face;
        face.a = edge.a;
        face.b = edge.b;
        face.kind = edge.type;
        face.uD = edge.uD;
        face.gN = edge.gN;
        face.k = edge.k;
        face.g = edge.g;
        model.faces.push_back(face);
    }
    return model;
}

bool DirichletMask::empty() const noexcept {
    return is_dirichlet.empty();
}

Count DirichletMask::size() const noexcept {
    return to_count(is_dirichlet.size());
}

bool DirichletMask::contains(Index node) const noexcept {
    return is_valid(node, is_dirichlet.size()) && is_dirichlet[to_size(node)] != 0;
}

DirichletMask build_dirichlet_mask(const BoundaryModel& boundary, Count dof_count) {
    DirichletMask mask;
    mask.is_dirichlet.assign(to_size(static_cast<Index>(dof_count)), 0);
    mask.value.assign(to_size(static_cast<Index>(dof_count)), Real(0));

    std::vector<Count> count(to_size(static_cast<Index>(dof_count)), Count{0});
    std::vector<Real> sum(to_size(static_cast<Index>(dof_count)), Real(0));

    for (const BoundaryFace& face : boundary.faces) {
        if (face.kind != BCType::Dirichlet) continue;
        add_dirichlet_node(face.a, face.uD, mask, count, sum);
        add_dirichlet_node(face.b, face.uD, mask, count, sum);
    }

    for (Index i = 0; i < static_cast<Index>(mask.is_dirichlet.size()); ++i) {
        if (count[to_size(i)] == Count{0}) continue;
        mask.is_dirichlet[to_size(i)] = 1;
        mask.value[to_size(i)] = sum[to_size(i)] / static_cast<Real>(count[to_size(i)]);
    }

    return mask;
}

DirichletMask build_dirichlet_mask(const FEMMesh& mesh) {
    return build_dirichlet_mask(make_boundary_model(mesh), mesh.dof_count_count());
}

} // namespace fem
