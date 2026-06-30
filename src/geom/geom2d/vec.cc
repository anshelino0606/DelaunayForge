#include "vec.h"
#include "math/math_.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

namespace fem::geom2d::vec {

double cross(const glm::dvec2& a, const glm::dvec2& b) {
    return a.x*b.y - a.y*b.x;
}

double angle(const glm::dvec2& u, const glm::dvec2& v) {
    const double nu = std::sqrt(u.x*u.x + u.y*u.y);
    const double nv = std::sqrt(v.x*v.x + v.y*v.y);
    if (nu <= 1e-30 || nv <= 1e-30) return 0.0;
    const double cs = (u.x*v.x + u.y*v.y) / (nu * nv);
    return math::safe_acos(cs);
}

double dist(const glm::dvec2& a, const glm::dvec2& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

double dist2(const glm::dvec2& a, const glm::dvec2& b) {
    return glm::distance2(a, b);
}

}