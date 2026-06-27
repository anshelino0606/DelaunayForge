#pragma once

#include <glm/vec2.hpp>

namespace fem::geom2d::vec {

double cross(const glm::dvec2& a, const glm::dvec2& b);
double angle(const glm::dvec2& u, const glm::dvec2& v);
double hypot(const glm::dvec2& a, const glm::dvec2& b);
double dist2(const glm::dvec2& a, const glm::dvec2& b);

}