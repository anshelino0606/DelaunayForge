#pragma once

#include "math/math_.h"
#include "geom/common_types_2d.h"

namespace fem {

namespace geom2d::loop {

constexpr double kValidationEps = 1e-9;

struct Bounds {
    double xmin = math::DMAX;
    double xmax = math::DMIN;
    double ymin = math::DMAX;
    double ymax = math::DMIN;

    [[nodiscard]] bool valid() const {
        return xmin <= xmax && ymin <= ymax;
    }
};

double signed_area(const std::vector<Point2D>& loop);
void normalize_boundary(std::vector<Point2D>& loop, bool make_clockwise);
Bounds compute_bounds(const std::vector<Point2D>& loop);
bool self_intersects(const std::vector<Point2D>& loop);
bool point_inside(const std::vector<Point2D>& loop, const glm::dvec2& point);
bool point_inside(const std::vector<Point2D>& all_points, const std::vector<int>& loop_indices, const glm::dvec2& point);

}

}