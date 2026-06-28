#pragma once

#include "geom/common_types_2d.h"

namespace fem::geom2d::query {

bool point_has_clearance_from_loop(const glm::dvec2& point, const std::vector<Point2D>& loop, double clearance_sq);
bool loop_has_clearance_from_loop(const std::vector<Point2D>& candidate, const std::vector<Point2D>& obstacle, double clearance_sq);
bool points_on_circle(const std::vector<Point2D>& pts, double eps = 1e-10);

}