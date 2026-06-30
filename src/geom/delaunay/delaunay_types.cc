#include "delaunay_types.h"

namespace fem {

FEM_DEFINE_STRUCT(DelaunayTriangulationResult);
FEM_BEGIN_PROPERTY_REGISTER(DelaunayTriangulationResult)
{
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, points);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, triangles);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, boundary_edges);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, tri2vert);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, tri_neighbors);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, vert2tri);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, edges);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, tri2edge);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, min_angle);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, median_angle);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, avg_angle);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, triangle_count);
    FEM_REGISTER_PROPERTY(DelaunayTriangulationResult, point_count);
}
FEM_END_PROPERTY_REGISTER(DelaunayTriangulationResult)

}