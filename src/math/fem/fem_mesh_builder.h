#ifndef FEM_MESH_BUILDER
#define FEM_MESH_BUILDER

#include "fem_mesh.h"

#include "delaunay_types.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace fem {

inline FEMMesh build_fem_mesh(const DelaunayTriangulationResult& R) {
    FEMMesh M;

    M.nodes.reserve(R.points.size());
    for (size_t i = 0; i < R.points.size(); ++i) {
        M.nodes.push_back({ R.points[i].x(), R.points[i].y(), (int)i });
    }

    M.elems.reserve(R.triangles.size());
    for (const auto& t : R.triangles) if (t.valid) {
        const auto& A = R.points[t.v[0]];
        const auto& B = R.points[t.v[1]];
        const auto& C = R.points[t.v[2]];
        double area = 0.5 * std::abs(
            (B.x() - A.x()) * (C.y() - A.y()) - (C.x() - A.x()) * (B.y() - A.y())
        );
        // Skip degenerate triangles to avoid divide-by-zero in gradient computation.
        if (area <= 1e-14) continue;
        M.elems.push_back({ { t.v[0], t.v[1], t.v[2] }, area });
    }

    for (const auto& e : R.edges) if (e.on_boundary) {
        FEMMesh::EdgeBC bc{};
        bc.a = std::min(e.a, e.b);
        bc.b = std::max(e.a, e.b);

        bc.type = e.bc.type;

        switch (e.bc.type) {
            case fem::BCType::Dirichlet:
                bc.uD   = e.bc.value;
                break;

            case fem::BCType::Neumann:
                bc.gN   = e.bc.value;
                break;

            case fem::BCType::Robin:
                bc.k    = e.bc.value;       // k
                bc.g    = e.bc.value_beta;  // g
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

inline FEMMesh build_fem_mesh_all_boundary_dirichlet(
    const DelaunayTriangulationResult& R,
    const std::function<double(double, double)>& u_dirichlet
) {
    FEMMesh M;

    M.nodes.reserve(R.points.size());
    for (size_t i = 0; i < R.points.size(); ++i) {
        M.nodes.push_back({ R.points[i].x(), R.points[i].y(), (int)i });
    }

    M.elems.reserve(R.triangles.size());
    for (const auto& t : R.triangles) if (t.valid) {
        const auto& A = R.points[t.v[0]];
        const auto& B = R.points[t.v[1]];
        const auto& C = R.points[t.v[2]];
        double area = 0.5 * std::abs(
            (B.x() - A.x()) * (C.y() - A.y()) - (C.x() - A.x()) * (B.y() - A.y())
        );
        if (area <= 1e-14) continue;
        M.elems.push_back({ { t.v[0], t.v[1], t.v[2] }, area });
    }

    M.edges_bc.reserve(R.edges.size());
    for (const auto& e : R.edges) if (e.on_boundary) {
        if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;

        const double mx = 0.5 * (R.points[e.a].x() + R.points[e.b].x());
        const double my = 0.5 * (R.points[e.a].y() + R.points[e.b].y());
        const double uD = u_dirichlet ? u_dirichlet(mx, my) : 0.0;

        FEMMesh::EdgeBC bc{};
        bc.a = std::min(e.a, e.b);
        bc.b = std::max(e.a, e.b);
        bc.type = fem::BCType::Dirichlet;
        bc.uD = uD;
        M.edges_bc.push_back(bc);
    }

    return M;
}

}

#endif // FEM_MESH_BUILDER
