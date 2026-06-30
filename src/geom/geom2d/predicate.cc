#include "predicate.h"
#include <algorithm>

namespace fem::geom2d::predicate {

double orient(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    long double ax = a.x, ay = a.y;
    long double bx = b.x, by = b.y;
    long double cx = c.x, cy = c.y;
    long double det = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    return static_cast<double>(det);
}

int32_t orient_sign(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, double eps) {
    double v = orient(a, b, c);
    if (v > eps) return +1;
    if (v < -eps) return -1;
    return 0;
}

double incircle_val(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d) {
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

int32_t incircle_ccw_scaled_strict(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d) {
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

int32_t incircle_ccw(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c, const glm::dvec2& d, double eps) {
    const long double det = incircle_val(a,b,c,d);

    const long double m = std::max<long double>({
        1.0L,
        fabsl((long double)a.x - (long double)d.x), fabsl((long double)a.y - (long double)d.y),
        fabsl((long double)b.x - (long double)d.x), fabsl((long double)b.y - (long double)d.y),
        fabsl((long double)c.x - (long double)d.x), fabsl((long double)c.y - (long double)d.y)
    });

    const long double final_eps = (long double)eps * m*m*m*m;

    if (det >  final_eps) return +1;
    if (det < -final_eps) return -1;
    return 0;
}

}