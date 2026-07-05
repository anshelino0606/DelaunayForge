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

    bool valid_vertex(int local_idx, std::size_t point_count) const noexcept {
        return v[local_idx] >= 0 && static_cast<std::size_t>(v[local_idx]) < point_count;
    }

    /// Returns true when all three vertex indices are valid for a points array of size `point_count`.
    bool valid_vertices(std::size_t point_count) const noexcept {
        return valid_vertex(0, point_count)
            && valid_vertex(1, point_count)
            && valid_vertex(2, point_count);
    }


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

    /// Returns true when both endpoint indices are valid for a points array of size `point_count`.
    bool valid_vertices(std::size_t point_count) const noexcept {
        return a >= 0 && static_cast<std::size_t>(a) < point_count
            && b >= 0 && static_cast<std::size_t>(b) < point_count;
    }
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

    /// Returns true when both endpoint indices are valid for a points array of size `point_count`.
    bool valid_vertices(std::size_t point_count) const noexcept {
        return a >= 0 && static_cast<std::size_t>(a) < point_count
            && b >= 0 && static_cast<std::size_t>(b) < point_count;
    }
};

using PackedEdge = std::uint64_t;

template<typename T>
[[nodiscard]] constexpr PackedEdge pack_edge(T a, T b) noexcept {
    const std::uint32_t ua = static_cast<std::uint32_t>(a < b ? a : b);
    const std::uint32_t ub = static_cast<std::uint32_t>(a < b ? b : a);
    return (static_cast<PackedEdge>(ua) << 32) | static_cast<PackedEdge>(ub);
}

struct PackedEdgeHash {
    [[nodiscard]] std::size_t operator()(PackedEdge k) const noexcept;
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