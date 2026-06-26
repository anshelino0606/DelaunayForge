#include "segment.h"
#include "predicate.h"
#include "geom/common_types_2d.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include <algorithm>

namespace fem::geom2d::segment {

double point_segment_dist2(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, double eps) {
    const glm::dvec2 ab = b - a;
    const glm::dvec2 ap = p - a;
    const double ab2 = glm::dot(ab, ab);
    if (ab2 <= eps) return glm::distance2(p, a);

    double t = glm::dot(ap, ap) / ab2;
    t = std::clamp(t, 0.0, 1.0);
    const glm::dvec2 q = a + t * ab;
    return glm::distance2(p, q);
}

double point_segment_dist2(const glm::dvec2& p, const Point2D& a, const Point2D& b, double eps) {
    return point_segment_dist2(p, a.p, b.p, eps);
}

bool point_on_segment(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, double eps) {
    if (std::abs(pred::orient(a, b, p)) > eps) return false;

    const double xmin = std::min(a.x, b.x) - eps;
    const double xmax = std::max(a.x, b.x) + eps;
    const double ymin = std::min(a.y, b.y) - eps;
    const double ymax = std::max(a.y, b.y) + eps;
    return p.x >= xmin && p.x <= xmax && p.y >= ymin && p.y <= ymax;
}

bool intersect_or_touch(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d, double eps) {
    const double o1 = pred::orient(a, b, c);
    const double o2 = pred::orient(a, b, d);
    const double o3 = pred::orient(c, d, a);
    const double o4 = pred::orient(c, d, b);

    const int s1 = math::sign_eps(o1, eps);
    const int s2 = math::sign_eps(o2, eps);
    const int s3 = math::sign_eps(o3, eps);
    const int s4 = math::sign_eps(o4, eps);

    if (s1 * s2 < 0 && s3 * s4 < 0) return true;
    if (s1 == 0 && point_on_segment(c, a, b, eps)) return true;
    if (s2 == 0 && point_on_segment(d, a, b, eps)) return true;
    if (s3 == 0 && point_on_segment(a, c, d, eps)) return true;
    if (s4 == 0 && point_on_segment(b, c, d, eps)) return true;
    return false;
}

double segment_segment_dist2(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d, double eps) {
    if (intersect_or_touch(a, b, c, d, eps)) {
        return 0.0;
    }

    return std::min({
        point_segment_dist2(a, c, d, eps),
        point_segment_dist2(b, c, d, eps),
        point_segment_dist2(c, a, b, eps),
        point_segment_dist2(d, a, b, eps),
    });
}

}