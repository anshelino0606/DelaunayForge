#include "geometry_2d.h"
#include "math/math_.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <cmath>
#include <algorithm>

namespace fem {

double Geometry2D::cross(const glm::dvec2& a, const glm::dvec2& b) {
    return a.x*b.y - a.y*b.x;
}

double Geometry2D::cross(const Point2D& a, const Point2D& b) {
    return cross(a.p, b.p);
}

bool Geometry2D::point_in_triangle(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    const glm::dvec2 ab = b - a, bc = c - b, ca = a - c;
    const glm::dvec2 ap = p - a, bp = p - b, cp = p - c;

    const double c1 = cross(ab, ap);
    const double c2 = cross(bc, bp);
    const double c3 = cross(ca, cp);

    const bool has_neg = (c1 < 0.0) || (c2 < 0.0) || (c3 < 0.0);
    const bool has_pos = (c1 > 0.0) || (c2 > 0.0) || (c3 > 0.0);
    return !(has_neg && has_pos);
}

bool Geometry2D::point_in_triangle(const glm::dvec2& p, const Point2D& a, const Point2D& b, const Point2D& c) {
    return point_in_triangle(p, a.p, b.p, c.p);
}

double Geometry2D::tri_area(const Point2D& a, const Point2D& b, const Point2D& c) {
    return 0.5 * std::abs(cross(b.p - a.p, c.p - a.p));
}

void Geometry2D::tri_shape_coefficients(const glm::dvec2& p0, const glm::dvec2& p1, const glm::dvec2& p2, glm::dvec3& out_b, glm::dvec3& out_c) {
    out_b[0] = p1.y - p2.y; out_c[0] = p2.x - p1.x;
    out_b[1] = p2.y - p0.y; out_c[1] = p0.x - p2.x;
    out_b[2] = p0.y - p1.y; out_c[2] = p1.x - p0.x;
}

void Geometry2D::tri_shape_coefficients(const Point2D& p0, const Point2D& p1, const Point2D& p2, glm::dvec3& out_b, glm::dvec3& out_c) {
    tri_shape_coefficients(p0.p, p1.p, p2.p, out_b, out_c);
}

glm::dvec2 Geometry2D::tri_centroid(const glm::dvec2& p0, const glm::dvec2& p1, const glm::dvec2& p2) {
    return glm::dvec2{
        (p0.x + p1.x + p2.x) / 3.0,
        (p0.y + p1.y + p2.y) / 3.0
    };
}

glm::dvec2 Geometry2D::tri_centroid(const Point2D& p0, const Point2D& p1, const Point2D& p2) {
    return tri_centroid(p0.p, p1.p, p2.p);
}

glm::dvec2 Geometry2D::tri_centroid(double p0_x, double p0_y, double p1_x, double p1_y, double p2_x, double p2_y) {
    return tri_centroid(glm::dvec2{p0_x, p0_y}, glm::dvec2{p1_x, p1_y}, glm::dvec2(p2_x, p2_y));
}

double Geometry2D::dist2(const glm::dvec2& a, const glm::dvec2& b) {
    return glm::distance2(a, b);
}

double Geometry2D::point_segment_dist2(const glm::dvec2& p, const Point2D& a, const Point2D& b) {
    const glm::dvec2 ab = b.p - a.p;
    const glm::dvec2 ap = p - a.p;
    const double ab2 = ab.x*ab.x + ab.y*ab.y;
    if (ab2 <= 1e-30) return glm::distance2(p, a.p);

    double t = (ap.x*ab.x + ap.y*ab.y) / ab2;
    t = std::clamp(t, 0.0, 1.0);
    const glm::dvec2 q = a.p + t * ab;
    return glm::distance2(p, q);
}

double Geometry2D::angle(const glm::dvec2& u, const glm::dvec2& v) {
    const double nu = std::sqrt(u.x*u.x + u.y*u.y);
    const double nv = std::sqrt(v.x*v.x + v.y*v.y);
    if (nu <= 1e-30 || nv <= 1e-30) return 0.0;
    const double cs = (u.x*v.x + u.y*v.y) / (nu * nv);
    return Math::safe_acos(cs);
}

double Geometry2D::min_angle_deg(const Point2D& a, const Point2D& b, const Point2D& c) {
    const double A = angle(b.p - a.p, c.p - a.p);
    const double B = angle(a.p - b.p, c.p - b.p);
    const double C = angle(a.p - c.p, b.p - c.p);

    const double m = std::min(A, std::min(B, C));
    return glm::degrees(m);
}

double Geometry2D::hypot(const Point2D& a, const Point2D& b) {
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

}