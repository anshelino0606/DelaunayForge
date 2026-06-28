#include "loop.h"
#include "segment.h"

namespace fem::geom2d::loop {

double signed_area(const std::vector<Point2D>& loop) {
    if (loop.size() < 3) return 0.0;

    double area = 0.0;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const Point2D& p = loop[i];
        const Point2D& q = loop[(i + 1) % loop.size()];
        area += p.x() * q.y() - q.x() * p.y();
    }
    return 0.5 * area;
}

void normalize_boundary(std::vector<Point2D>& loop, bool make_clockwise) {
    if (make_clockwise ? (signed_area(loop) > 0.0) : (signed_area(loop) < 0.0)) {
        std::reverse(loop.begin(), loop.end());
    }

    for (std::size_t i = 0; i < loop.size(); ++i) {
        loop[i].id = static_cast<int>(i);
        loop[i].on_boundary = true;
    }
}

Bounds compute_bounds(const std::vector<Point2D>& loop) {
    Bounds bounds;
    for (const Point2D& point : loop) {
        bounds.xmin = std::min(bounds.xmin, point.x());
        bounds.xmax = std::max(bounds.xmax, point.x());
        bounds.ymin = std::min(bounds.ymin, point.y());
        bounds.ymax = std::max(bounds.ymax, point.y());
    }
    return bounds;
}

bool self_intersects(const std::vector<Point2D>& loop) {
    if (loop.size() < 4) return false;

    for (std::size_t i = 0; i < loop.size(); ++i) {
        const glm::dvec2 a = loop[i].p;
        const glm::dvec2 b = loop[(i + 1) % loop.size()].p;

        for (std::size_t j = i + 1; j < loop.size(); ++j) {
            const std::size_t i_next = (i + 1) % loop.size();
            const std::size_t j_next = (j + 1) % loop.size();

            if (i == j || i == j_next || i_next == j || i_next == j_next) {
                continue;
            }

            const glm::dvec2 c = loop[j].p;
            const glm::dvec2 d = loop[j_next].p;
            if (segment::intersect_or_touch(a, b, c, d, kValidationEps)) {
                return true;
            }
        }
    }

    return false;
}

bool ray_intersects_edge(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& point) {
    const bool crosses_y = (a.y > point.y) != (b.y > point.y);
    if (!crosses_y) return false;

    const double denom = (b.y - a.y) + 1e-300;
    const double x_on_edge = (b.x - a.x) * (point.y - a.y) / denom + a.x;
    return point.x < x_on_edge;
}

bool point_inside(const std::vector<Point2D>& loop, const glm::dvec2& point) {
    if (loop.size() < 3) return false;

    bool inside = false;
    std::size_t j = loop.size() - 1;
    for (std::size_t i = 0; i < loop.size(); j = i++) {
        if (ray_intersects_edge(loop[i], loop[j], point)) {
            inside = !inside;
        }
    }
    return inside;
}

bool point_inside(const std::vector<Point2D>& all_points, const std::vector<int>& loop_indices, const glm::dvec2& point) {
    if (loop_indices.size() < 3) return true;

    bool inside = false;
    std::size_t j = loop_indices.size() - 1;

    for (std::size_t i = 0; i < loop_indices.size(); j = i++) {
        const Point2D& a = all_points[static_cast<std::size_t>(loop_indices[i])];
        const Point2D& b = all_points[static_cast<std::size_t>(loop_indices[j])];

        if (ray_intersects_edge(a, b, point)) {
            inside = !inside;
        }
    }
    return inside;
}

}