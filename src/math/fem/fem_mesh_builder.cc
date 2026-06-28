#include "fem_mesh_builder.h"

#include <algorithm>
#include <cmath>

namespace fem {

FEMMesh build_fem_mesh(const DelaunayTriangulationResult& R) {
    FEMMesh M;

    M.nodes.reserve(R.points.size());
    for (std::size_t i = 0; i < R.points.size(); ++i) {
        M.nodes.push_back({R.points[i].x(), R.points[i].y(), to_index(i)});
    }

    M.elems.reserve(R.triangles.size());
    for (const Tri& t : R.triangles) {
        if (!t.valid) continue;
        const Point2D& A = R.points[t.v[0]];
        const Point2D& B = R.points[t.v[1]];
        const Point2D& C = R.points[t.v[2]];
        const double area = 0.5 * std::abs(
            (B.x() - A.x()) * (C.y() - A.y()) - (C.x() - A.x()) * (B.y() - A.y())
        );
        if (area <= 1e-14) continue;
        M.elems.push_back({{to_index_or_invalid(t.v[0]), to_index_or_invalid(t.v[1]), to_index_or_invalid(t.v[2])}, area});
    }

    for (const auto& e : R.edges) {
        if (!e.on_boundary) continue;
        FEMMesh::EdgeBC bc{};
        bc.a = to_index_or_invalid(std::min(e.a, e.b));
        bc.b = to_index_or_invalid(std::max(e.a, e.b));
        bc.type = e.bc.type;

        switch (e.bc.type) {
            case fem::BCType::Dirichlet:
                bc.uD = e.bc.value;
                break;
            case fem::BCType::Neumann:
                bc.gN = e.bc.value;
                break;
            case fem::BCType::Robin:
                bc.k = e.bc.value;
                bc.g = e.bc.value_beta;
                break;
            case fem::BCType::None:
            default:
                break;
        }

        if (bc.type != fem::BCType::None) {
            M.edges_bc.push_back(bc);
        }
    }

    return M;
}

FEMMesh build_fem_mesh_all_boundary_dirichlet(
    const DelaunayTriangulationResult& R,
    const std::function<double(double, double)>& u_dirichlet
) {
    FEMMesh M;

    M.nodes.reserve(R.points.size());
    for (std::size_t i = 0; i < R.points.size(); ++i) {
        M.nodes.push_back({R.points[i].x(), R.points[i].y(), to_index(i)});
    }

    M.elems.reserve(R.triangles.size());
    for (const Tri& t : R.triangles) {
        if (!t.valid) continue;
        const Point2D& A = R.points[t.v[0]];
        const Point2D& B = R.points[t.v[1]];
        const Point2D& C = R.points[t.v[2]];
        const double area = 0.5 * std::abs(
            (B.x() - A.x()) * (C.y() - A.y()) - (C.x() - A.x()) * (B.y() - A.y())
        );
        if (area <= 1e-14) continue;
        M.elems.push_back({{to_index_or_invalid(t.v[0]), to_index_or_invalid(t.v[1]), to_index_or_invalid(t.v[2])}, area});
    }

    M.edges_bc.reserve(R.edges.size());
    for (const EdgeInfo& e : R.edges) {
        if (!e.valid_vertices(R.points.size()) || !e.on_boundary) continue;

        const double mx = 0.5 * (R.points[e.a].x() + R.points[e.b].x());
        const double my = 0.5 * (R.points[e.a].y() + R.points[e.b].y());
        const double uD = u_dirichlet ? u_dirichlet(mx, my) : 0.0;

        FEMMesh::EdgeBC bc{};
        bc.a = to_index_or_invalid(std::min(e.a, e.b));
        bc.b = to_index_or_invalid(std::max(e.a, e.b));
        bc.type = fem::BCType::Dirichlet;
        bc.uD = uD;
        M.edges_bc.push_back(bc);
    }

    return M;
}

} // namespace fem
