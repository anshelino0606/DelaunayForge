#pragma once

#include "common_types_2d.h"

namespace fem {

struct DelaunayTriangulationConfig {
    float min_angle_threshold = 20.0f;
    bool enable_lloyd_smoothing = true;
    int lloyd_iterations = 3;
    bool enable_edge_flipping = true;
    double epsilon = 1e-12;

    bool enable_min_angle_refinement = false;
    int refine_max_steiner = 25;

    bool enable_sizing_refinement = false;
    int refine_sizing_max_steiner = 500;
    double density_refine_threshold = 1.25;
};

struct DelaunayTriangulationResult : public Struct {
    FEM_DECLARE_STRUCT(DelaunayTriangulationResult);
    FEM_DECLARE_PROPERTY_REGISTER(DelaunayTriangulationResult);

    std::vector<Point2D> points;
    std::vector<Tri> triangles;
    std::vector<Edge> boundary_edges;

    std::vector<glm::ivec3> tri2vert;       // triangle -> vertex indices
    std::vector<glm::ivec3> tri_neighbors;  // triangle -> neighbor triangle IDs
    std::vector<std::vector<int>> vert2tri; // vertex -> incident triangles

    std::vector<EdgeInfo> edges;            // unique edges
    std::vector<glm::ivec3> tri2edge;       // per tri, edge ids in (v0-v1, v1-v2, v2-v0)

    double min_angle = 0.0;
    double median_angle = 0.0;
    double avg_angle = 0.0;
    int triangle_count = 0;
    int point_count = 0;
};

}