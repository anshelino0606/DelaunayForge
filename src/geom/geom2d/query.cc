#include "query.h"
#include "segment.h"
#include "loop.h"
#include "vec.h"

namespace fem::geom2d::query {

bool point_has_clearance_from_loop(const glm::dvec2& point, const std::vector<Point2D>& loop, double clearance_sq) {
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const glm::dvec2 a = loop[i].p;
        const glm::dvec2 b = loop[(i + 1) % loop.size()].p;
        if (segment::point_segment_dist2(point, a, b, loop::kValidationEps) <= clearance_sq) {
            return false;
        }
    }
    return true;
}

bool loop_has_clearance_from_loop(const std::vector<Point2D>& candidate, const std::vector<Point2D>& obstacle, double clearance_sq) {
    for (std::size_t i = 0; i < candidate.size(); ++i) {
        const glm::dvec2 a = candidate[i].p;
        const glm::dvec2 b = candidate[(i + 1) % candidate.size()].p;
        for (std::size_t j = 0; j < obstacle.size(); ++j) {
            const glm::dvec2 c = obstacle[j].p;
            const glm::dvec2 d = obstacle[(j + 1) % obstacle.size()].p;
            if (segment::segment_segment_dist2(a, b, c, d, loop::kValidationEps) <= clearance_sq) {
                return false;
            }
        }
    }
    return true;
}

bool points_on_circle(const std::vector<Point2D>& pts, double eps) {
    if (pts.size() < 4) return false;

    glm::dvec2 A = pts[0].p, B = pts[1].p, C = pts[2].p;
    double d = 2.0 * (A.x*(B.y - C.y) + B.x*(C.y - A.y) + C.x*(A.y - B.y));
    if (std::abs(d) < eps) return false;
    double ux = ((A.x*A.x + A.y*A.y)*(B.y - C.y) +
                 (B.x*B.x + B.y*B.y)*(C.y - A.y) +
                 (C.x*C.x + C.y*C.y)*(A.y - B.y)) / d;
    double uy = ((A.x*A.x + A.y*A.y)*(C.x - B.x) +
                 (B.x*B.x + B.y*B.y)*(A.x - C.x) +
                 (C.x*C.x + C.y*C.y)*(B.x - A.x)) / d;
    glm::dvec2 O(ux, uy);
    double R = geom2d::vec::dist(A, {ux, uy});

    for (const Point2D& p : pts) {
        double r = geom2d::vec::dist(p, {ux, uy});
        if (std::abs(r - R) > 1e-6 * R) return false;
    }
    return true;
}

}