#pragma once

#include "math/math_.h"
#include "geom/geom2d/vec.h"
#include <glm/vec2.hpp>
#include <algorithm>

namespace fem::geom2d {

enum class Side {
    Unknown = 0,
    Left    = 1,
    Right   = 2,
    Bottom  = 3,
    Top     = 4
};

template<typename ScalarType>
struct TBoundingBox {
    using Vec = glm::vec<2, ScalarType, glm::defaultp>;

    Vec mins{math::DMAX};
    Vec maxs{math::DMIN};

    void update(const Vec& val) {
        mins.x = std::min(mins.x, val.x);
        mins.y = std::min(mins.y, val.y);
        maxs.x = std::max(maxs.x, val.x);
        maxs.y = std::max(maxs.y, val.y);
    }

    [[nodiscard]] bool contains(ScalarType x, ScalarType y) const noexcept {
        return x >= mins.x && x <= maxs.x && y >= mins.y && y <= maxs.y;
    }

    [[nodiscard]] bool contains(const Vec& val) const noexcept {
        return val.x >= mins.x && val.x <= maxs.x && val.y >= mins.y && val.y <= maxs.y;
    }

    ScalarType dx() const {
        return maxs.x - mins.x;
    }

    ScalarType dy() const {
        return maxs.y - mins.y;
    }

    ScalarType dmax() const {
        return std::max(dx(), dy());
    }

    ScalarType cx() const {
        return (maxs.x + mins.x) * 0.5;
    }

    ScalarType cy() const {
        return (maxs.y + mins.y) * 0.5;
    }

    ScalarType dist() const {
        return vec::dist(mins, maxs);
    }

    [[nodiscard]] Side classify_side(const Vec& a, const Vec& b, ScalarType eps = 1e-9) const noexcept {
        ScalarType tol = eps * std::max(1.0, dmax());

        if (std::abs(a.x - mins.x) <= tol && std::abs(b.x - mins.x) <= tol) return Side::Left;
        if (std::abs(a.x - maxs.x) <= tol && std::abs(b.x - maxs.x) <= tol) return Side::Right;
        if (std::abs(a.y - mins.y) <= tol && std::abs(b.y - mins.y) <= tol) return Side::Bottom;
        if (std::abs(a.y - maxs.y) <= tol && std::abs(b.y - maxs.y) <= tol) return Side::Top;
        return Side::Unknown;
    }

    [[nodiscard]] constexpr Vec outward_normal(Side side) const noexcept {
        switch (side) {
            case Side::Left:   return Vec{-1,  0};
            case Side::Right:  return Vec{ 1,  0};
            case Side::Bottom: return Vec{ 0, -1};
            case Side::Top:    return Vec{ 0,  1};
            case Side::Unknown:
            default: {
                return Vec{0, 0};
            }
        }
    }
};

using BoundingBox = TBoundingBox<Real>;

}