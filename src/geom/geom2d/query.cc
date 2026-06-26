#include "query.h"
#include "segment.h"
#include "loop.h"

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

}