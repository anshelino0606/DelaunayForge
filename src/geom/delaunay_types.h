// delaunay_types.h
#ifndef DELAUNAY_TYPES_H
#define DELAUNAY_TYPES_H

#include "core/object/object.h"
#include "core/object/property.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <glm/glm.hpp>
#include "math/fem/bc_value.h"

namespace fem {

struct Tri : public Struct {
    FEM_DECLARE_STRUCT(Tri);
    FEM_DECLARE_PROPERTY_REGISTER(Tri);

    glm::ivec3 v{ -1, -1, -1 };          // CCW vertex indices
    int id = -1;
    glm::ivec3 neighbors{ -1, -1, -1 };  // Adjacent triangle IDs
    bool valid = true;

    Tri() = default;
    Tri(int a, int b, int c, int triangle_id = -1)
        : v(a, b, c), id(triangle_id), valid(true) {}

    bool contains_vertex(int vertex) const {
        return (v.x == vertex) || (v.y == vertex) || (v.z == vertex);
    }

    int& operator[](int i)             { return v[i]; }
    const int& operator[](int i) const { return v[i]; }
};

struct Edge : public Struct {
    FEM_DECLARE_STRUCT(Edge);
    FEM_DECLARE_PROPERTY_REGISTER(Edge);
    
    int a = -1, b = -1;
    bool on_boundary = false;
    
    Edge() = default;
    Edge(int x, int y) : a(std::min(x,y)), b(std::max(x,y)) {}
};

inline Edge canon(int i, int j) { return Edge(i, j); }

struct EdgeHash {
    std::size_t operator()(const Edge& e) const noexcept {
        return (std::size_t(e.a) << 32) ^ std::size_t(e.b);
    }
};

inline bool operator==(const Edge& x, const Edge& y) { 
    return x.a == y.a && x.b == y.b; 
}

struct Point2D : public Struct {
    FEM_DECLARE_STRUCT(Point2D);
    FEM_DECLARE_PROPERTY_REGISTER(Point2D);

    glm::dvec2 p{0.0, 0.0};
    int id = -1;
    bool on_boundary = false;
    std::vector<int> incident_triangles;

    Point2D() = default;
    Point2D(double px, double py, int point_id = -1)
        : p(px, py), id(point_id) {}

    double x() const { return p.x; }
    double y() const { return p.y; }

    explicit operator glm::dvec2() const { return p; }
};


struct EdgeInfo : public Struct {
    FEM_DECLARE_STRUCT(EdgeInfo);
    FEM_DECLARE_PROPERTY_REGISTER(EdgeInfo);

    int a = -1, b = -1;
    int tri_left  = -1;
    int tri_right = -1;
    bool on_boundary = false;
    int boundary_tag = -1;

    enum class BCType : int {
        None      = 0,
        Dirichlet = 1,
        Neumann   = 2,
        Robin     = 3
    };
    fem::BoundaryValue bc = fem::BoundaryValue::none();
};


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


#endif // DELAUNAY_TYPES_H