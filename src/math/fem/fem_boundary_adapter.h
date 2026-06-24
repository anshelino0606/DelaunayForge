#ifndef FEM_BOUNDARY_ADAPTER_H
#define FEM_BOUNDARY_ADAPTER_H

#include "math/pde/boundary_model.h"
#include "math/fem/fem_mesh.h"
#include "math/fem/bc_value.h"
#include "math/types.h"

#include <algorithm>
#include <vector>

namespace fem {

[[nodiscard]] inline BoundaryKind boundary_kind_from_bc_type(BCType type) noexcept {
    switch (type) {
        case BCType::Dirichlet: return BoundaryKind::Dirichlet;
        case BCType::Neumann:   return BoundaryKind::Neumann;
        case BCType::Robin:     return BoundaryKind::Robin;
        case BCType::None:
        default:                return BoundaryKind::None;
    }
}

[[nodiscard]] inline BCType bc_type_from_boundary_kind(BoundaryKind kind) noexcept {
    switch (kind) {
        case BoundaryKind::Dirichlet: return BCType::Dirichlet;
        case BoundaryKind::Neumann:   return BCType::Neumann;
        case BoundaryKind::Robin:     return BCType::Robin;
        case BoundaryKind::None:
        default:                      return BCType::None;
    }
}

[[nodiscard]] inline BoundaryModel make_boundary_model(const FEMMesh& mesh) {
    BoundaryModel model;
    model.faces.reserve(mesh.edges_bc.size());
    for (const FEMMesh::EdgeBC& edge : mesh.edges_bc) {
        if (!is_valid(edge.a, mesh.nodes.size()) || !is_valid(edge.b, mesh.nodes.size())) {
            continue;
        }

        BoundaryFace face;
        face.a = edge.a;
        face.b = edge.b;
        face.kind = boundary_kind_from_bc_type(edge.type);
        face.uD = edge.uD;
        face.gN = edge.gN;
        face.k = edge.k;
        face.g = edge.g;
        model.faces.push_back(face);
    }
    return model;
}

struct DirichletMask {
    std::vector<std::uint8_t> is_dirichlet;
    std::vector<Real> value;

    [[nodiscard]] bool empty() const noexcept { return is_dirichlet.empty(); }
    [[nodiscard]] Count size() const noexcept { return to_count(is_dirichlet.size()); }

    [[nodiscard]] bool contains(Index node) const noexcept {
        return is_valid(node, is_dirichlet.size()) && is_dirichlet[to_size(node)] != 0;
    }
};

using DirichletData = DirichletMask;

[[nodiscard]] inline DirichletMask build_dirichlet_mask(const BoundaryModel& boundary, Count dof_count) {
    DirichletMask mask;
    mask.is_dirichlet.assign(to_size(static_cast<Index>(dof_count)), 0);
    mask.value.assign(to_size(static_cast<Index>(dof_count)), Real(0));

    std::vector<Count> count(to_size(static_cast<Index>(dof_count)), Count{0});
    std::vector<Real> sum(to_size(static_cast<Index>(dof_count)), Real(0));

    auto add_node = [&](Index node, Real value) {
        if (!is_valid(node, mask.is_dirichlet.size())) return;
        count[to_size(node)] += Count{1};
        sum[to_size(node)] += value;
    };

    for (const BoundaryFace& face : boundary.faces) {
        if (face.kind != BoundaryKind::Dirichlet) continue;
        add_node(face.a, face.uD);
        add_node(face.b, face.uD);
    }

    for (Index i = 0; i < static_cast<Index>(mask.is_dirichlet.size()); ++i) {
        if (count[to_size(i)] == Count{0}) continue;
        mask.is_dirichlet[to_size(i)] = 1;
        mask.value[to_size(i)] = sum[to_size(i)] / static_cast<Real>(count[to_size(i)]);
    }

    return mask;
}

[[nodiscard]] inline DirichletMask build_dirichlet_mask(const FEMMesh& mesh) {
    return build_dirichlet_mask(make_boundary_model(mesh), mesh.dof_count_count());
}


} // namespace fem

#endif // FEM_BOUNDARY_ADAPTER_H
