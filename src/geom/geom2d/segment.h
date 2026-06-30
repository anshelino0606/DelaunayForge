#pragma once

#include <glm/vec2.hpp>

namespace fem {

struct Point2D;

namespace geom2d::segment {

double point_segment_dist(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, double eps = 1e-30);
double point_segment_dist2(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, double eps = 1e-30);
bool point_on_segment(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, double eps = 1e-9);
bool intersect(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d, double eps = 1e-9);
bool intersect_or_touch(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d, double eps = 1e-9);
double segment_segment_dist2(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d, double eps = 1e-9);

}

}