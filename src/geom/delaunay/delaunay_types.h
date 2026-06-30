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

    /// Returns true if `idx` is a valid point index.
    [[nodiscard]] bool valid_point(int idx) const noexcept {
        return idx >= 0 && static_cast<std::size_t>(idx) < points.size();
    }
    
    /// Returns true if `idx` refers to valid triangle
    [[nodiscard]] bool valid_triangle(int idx) const noexcept {
        if (idx < 0 || static_cast<std::size_t>(idx) >= triangles.size()) 
            return false;

        const Tri& tri = triangles[idx];
        if (!tri.valid) 
            return false;

        return tri.valid_vertices(points.size());
    }
    
    /// Returns true if `idx` refers to a valid edge
    [[nodiscard]] bool valid_edge(int idx) const noexcept {
        if (idx < 0 || static_cast<std::size_t>(idx) >= edges.size())
            return false;

        const EdgeInfo& edge = edges[idx];
        return edge.valid_vertices(points.size());
    }
};

}