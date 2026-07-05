#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace fem::geom2d::tri {

bool point_in_triangle(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c);

bool barycentric_coords(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, glm::dvec3& out_bary);
bool barycentric_in_triangle(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, glm::dvec3& out_bary, double eps = 1e-12);

double area(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c);

void shape_coefficients(const glm::dvec2& p0, const glm::dvec2& p1, const glm::dvec2& p2, glm::dvec3& out_b, glm::dvec3& out_c);

glm::dvec2 centroid(const glm::dvec2& p0, const glm::dvec2& p1, const glm::dvec2& p2);
glm::dvec2 centroid(double p0_x, double p0_y, double p1_x, double p1_y, double p2_x, double p2_y);

glm::dvec2 circumcenter(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C);
glm::dvec2 incenter(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C);
bool is_obtuse(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C);

// move "t" toward incenter (0=cc, 1=ic). t works well in range [0.3, 0.5]
glm::dvec2 offcenter(const glm::dvec2& cc, const glm::dvec2& ic, double t = 0.35);

double min_angle_deg(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c);

}