#pragma once

#include "core/object/object.h"
#include "core/object/property.h"
#include "math/fem/bc_value.h"
#include "math/math_.h"

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

inline bool operator==(const Edge& x, const Edge& y) { 
    return x.a == y.a && x.b == y.b; 
}

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

    bool operator<(const Point2D& other) const {
        if (!math::equals(p.x, other.p.x)) {
            return p.x < other.p.x;
        }
        return p.y < other.p.y;
    }

    bool operator==(const Point2D& other) const {
        return math::equals(p.x, other.p.x) && math::equals(p.y, other.p.y);
    }
};

}