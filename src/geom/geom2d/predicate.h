#pragma once

#include <glm/vec2.hpp>

namespace fem {

namespace geom2d::predicate {

// Robust orientation predicate with epsilon handling
double orient(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c);

int32_t orient_sign(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, double eps = 1e-15);

// Robust incircle predicate
double incircle_val(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d);
int32_t incircle_ccw_scaled_strict(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d);
int32_t incircle_ccw(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d, double eps = 1e-18);

}

namespace geom2d {

namespace pred = predicate;

}

}