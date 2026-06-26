#include "tri.h"
#include "vec.h"
#include "geom/common_types_2d.h"

namespace fem::geom2d::tri {

bool point_in_triangle(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    const glm::dvec2 ab = b - a, bc = c - b, ca = a - c;
    const glm::dvec2 ap = p - a, bp = p - b, cp = p - c;

    const double c1 = vec::cross(ab, ap);
    const double c2 = vec::cross(bc, bp);
    const double c3 = vec::cross(ca, cp);

    const bool has_neg = (c1 < 0.0) || (c2 < 0.0) || (c3 < 0.0);
    const bool has_pos = (c1 > 0.0) || (c2 > 0.0) || (c3 > 0.0);
    return !(has_neg && has_pos);
}

bool point_in_triangle(const glm::dvec2& p, const Point2D& a, const Point2D& b, const Point2D& c) {
    return point_in_triangle(p, a.p, b.p, c.p);
}

double area(const Point2D& a, const Point2D& b, const Point2D& c) {
    return 0.5 * std::abs(vec::cross(b.p - a.p, c.p - a.p));
}

void shape_coefficients(const glm::dvec2& p0, const glm::dvec2& p1, const glm::dvec2& p2, glm::dvec3& out_b, glm::dvec3& out_c) {
    out_b[0] = p1.y - p2.y; out_c[0] = p2.x - p1.x;
    out_b[1] = p2.y - p0.y; out_c[1] = p0.x - p2.x;
    out_b[2] = p0.y - p1.y; out_c[2] = p1.x - p0.x;
}

void shape_coefficients(const Point2D& p0, const Point2D& p1, const Point2D& p2, glm::dvec3& out_b, glm::dvec3& out_c) {
    shape_coefficients(p0.p, p1.p, p2.p, out_b, out_c);
}

glm::dvec2 centroid(const glm::dvec2& p0, const glm::dvec2& p1, const glm::dvec2& p2) {
    return glm::dvec2{
        (p0.x + p1.x + p2.x) / 3.0,
        (p0.y + p1.y + p2.y) / 3.0
    };
}

glm::dvec2 centroid(const Point2D& p0, const Point2D& p1, const Point2D& p2) {
    return centroid(p0.p, p1.p, p2.p);
}

glm::dvec2 centroid(double p0_x, double p0_y, double p1_x, double p1_y, double p2_x, double p2_y) {
    return centroid(glm::dvec2{p0_x, p0_y}, glm::dvec2{p1_x, p1_y}, glm::dvec2(p2_x, p2_y));
}

glm::dvec2 circumcenter(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C) {
    glm::dvec2 a = B - A, b = C - A;
    double aa = glm::dot(a,a), bb = glm::dot(b,b);
    double d  = 2.0 * (a.x*b.y - a.y*b.x);
    if (std::abs(d) < 1e-20) return centroid(A, B, C); // fallback
    glm::dvec2 u((bb*a.y - aa*b.y)/d, (aa*b.x - bb*a.x)/d);
    return A + u;
}

glm::dvec2 incenter(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C) {
    double a = glm::length(B - C);
    double b = glm::length(A - C);
    double c = glm::length(A - B);
    double s = a + b + c;
    return (a*A + b*B + c*C) / std::max(1e-30, s);
}

bool is_obtuse(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C) {
    auto obt = [](const glm::dvec2& p, const glm::dvec2& q, const glm::dvec2& r){
        return glm::dot(p - q, r - q) < 0.0; // obtuse at q if (p-q)·(r-q) < 0
    };
    return obt(A,B,C) || obt(B,C,A) || obt(C,A,B);
}

glm::dvec2 offcenter(const glm::dvec2& cc, const glm::dvec2& ic, double t) {
    return (1.0 - t)*cc + t*ic;
}

double min_angle_deg(const Point2D& a, const Point2D& b, const Point2D& c) {
    const double A = vec::angle(b.p - a.p, c.p - a.p);
    const double B = vec::angle(a.p - b.p, c.p - b.p);
    const double C = vec::angle(a.p - c.p, b.p - c.p);

    const double m = std::min(A, std::min(B, C));
    return glm::degrees(m);
}

}