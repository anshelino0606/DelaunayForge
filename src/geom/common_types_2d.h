#pragma once

#include "core/object/object.h"
#include "core/object/property.h"
#include "math/fem/bc_value.h"
#include "math/math_.h"

namespace fem {

struct Tri : public Struct {
    FEM_DECLARE_STRUCT(Tri);
    FEM_DECLARE_PROPERTY_REGISTER(Tri);

    static constexpr int32_t sInvalidIndex = -1;

    glm::ivec3 v{ sInvalidIndex };          // CCW vertex indices
    int32_t id = sInvalidIndex;
    glm::ivec3 neighbors{ sInvalidIndex };  // Adjacent triangle IDs
    bool valid = true;

    Tri() = default;
    Tri(int32_t a, int32_t b, int32_t c, int32_t triangle_id = sInvalidIndex)
        : v(a, b, c), id(triangle_id), valid(true) {}

    bool contains_vertex(int vertex) const {
        return (v.x == vertex) || (v.y == vertex) || (v.z == vertex);
    }
};

struct Edge : public Struct {
    FEM_DECLARE_STRUCT(Edge);
    FEM_DECLARE_PROPERTY_REGISTER(Edge);

    static constexpr int32_t sInvalidIndex = -1;
    
    int32_t a = sInvalidIndex; 
    int32_t b = sInvalidIndex;
    bool on_boundary = false;
    
    Edge() = default;
    Edge(int32_t x, int32_t y) : a(std::min(x,y)), b(std::max(x,y)) {}
};

inline bool operator==(const Edge& x, const Edge& y) { 
    return x.a == y.a && x.b == y.b; 
}

struct EdgeInfo : public Struct {
    FEM_DECLARE_STRUCT(EdgeInfo);
    FEM_DECLARE_PROPERTY_REGISTER(EdgeInfo);

    static constexpr int32_t sInvalidIndex = -1;

    int32_t a = sInvalidIndex; 
    int32_t b = sInvalidIndex;
    int32_t tri_left  = sInvalidIndex;
    int32_t tri_right = sInvalidIndex;
    bool on_boundary = false;
    int32_t boundary_tag = sInvalidIndex;

    BoundaryValue bc = BoundaryValue::none();
};

struct Point2D : public Struct {
    FEM_DECLARE_STRUCT(Point2D);
    FEM_DECLARE_PROPERTY_REGISTER(Point2D);

    static constexpr int32_t sInvalidIndex = -1;

    glm::dvec2 p{0.0, 0.0};
    int32_t id = sInvalidIndex;
    bool on_boundary = false;
    std::vector<int32_t> incident_triangles;

    Point2D() = default;
    Point2D(double px, double py, int32_t point_id = sInvalidIndex)
        : p(px, py), id(point_id) {}

    double x() const { return p.x; }
    double y() const { return p.y; }

    operator const glm::dvec2&() const { return p; }

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