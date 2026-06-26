#pragma once

#include <glm/vec2.hpp>

namespace fem {

struct Point2D;

namespace geom2d::vec {

double cross(const glm::dvec2& a, const glm::dvec2& b);
double cross(const Point2D& a, const Point2D& b);
double angle(const glm::dvec2& u, const glm::dvec2& v);
double hypot(const Point2D& a, const Point2D& b);
double dist2(const glm::dvec2& a, const glm::dvec2& b);

}

}