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

double Geometry2D::orient(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    long double ax = a.x, ay = a.y;
    long double bx = b.x, by = b.y;
    long double cx = c.x, cy = c.y;
    long double det = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    return static_cast<double>(det);
}

int32_t Geometry2D::orient_sign(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, double eps) {
    double v = orient(a, b, c);
    if (v > eps) return +1;
    if (v < -eps) return -1;
    return 0;
}

double Geometry2D::incircle_val(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d) {
    const long double ax = (long double)a.x - (long double)d.x;
    const long double ay = (long double)a.y - (long double)d.y;
    const long double bx = (long double)b.x - (long double)d.x;
    const long double by = (long double)b.y - (long double)d.y;
    const long double cx = (long double)c.x - (long double)d.x;
    const long double cy = (long double)c.y - (long double)d.y;

    const long double a2 = ax*ax + ay*ay;
    const long double b2 = bx*bx + by*by;
    const long double c2 = cx*cx + cy*cy;

    return a2 * (bx*cy - by*cx)
         - b2 * (ax*cy - ay*cx)
         + c2 * (ax*by - ay*bx);
}

int32_t Geometry2D::incircle_ccw_scaled_strict(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d) {
    const long double det = incircle_val(a,b,c,d);
    const long double m = std::max<long double>({
        1.0L,
        fabsl((long double)a.x - (long double)d.x), fabsl((long double)a.y - (long double)d.y),
        fabsl((long double)b.x - (long double)d.x), fabsl((long double)b.y - (long double)d.y),
        fabsl((long double)c.x - (long double)d.x), fabsl((long double)c.y - (long double)d.y)
    });
    const long double eps = 1e-18L * m*m*m*m;

    if (det >  eps) return +1;
    if (det < -eps) return -1;
    return 0;
}

// Why do we need eps param here?
int32_t Geometry2D::incircle_ccw(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d, double eps) {
    const long double det = incircle_val(a,b,c,d);

    const long double m = std::max<long double>({
        1.0L,
        fabsl((long double)a.x - (long double)d.x), fabsl((long double)a.y - (long double)d.y),
        fabsl((long double)b.x - (long double)d.x), fabsl((long double)b.y - (long double)d.y),
        fabsl((long double)c.x - (long double)d.x), fabsl((long double)c.y - (long double)d.y)
    });

    const long double default_eps = 1e-18L * m*m*m*m;

    if (det >  default_eps) return +1;
    if (det < -default_eps) return -1;
    return 0;
}

glm::dvec2 Geometry2D::circumcenter(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C) {
    glm::dvec2 a = B - A, b = C - A;
    double aa = glm::dot(a,a), bb = glm::dot(b,b);
    double d  = 2.0 * (a.x*b.y - a.y*b.x);
    if (std::abs(d) < 1e-20) return Geometry2D::tri_centroid(A, B, C); // fallback
    glm::dvec2 u((bb*a.y - aa*b.y)/d, (aa*b.x - bb*a.x)/d);
    return A + u;
}

glm::dvec2 Geometry2D::incenter(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C) {
    double a = glm::length(B - C);
    double b = glm::length(A - C);
    double c = glm::length(A - B);
    double s = a + b + c;
    return (a*A + b*B + c*C) / std::max(1e-30, s);
}

bool Geometry2D::is_obtuse(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C) {
    auto obt = [](const glm::dvec2& p, const glm::dvec2& q, const glm::dvec2& r){
        return glm::dot(p - q, r - q) < 0.0; // obtuse at q if (p-q)·(r-q) < 0
    };
    return obt(A,B,C) || obt(B,C,A) || obt(C,A,B);
}

glm::dvec2 Geometry2D::offcenter(const glm::dvec2& cc, const glm::dvec2& ic, double t) {
    return (1.0 - t)*cc + t*ic;
}

}