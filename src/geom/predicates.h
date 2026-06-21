#ifndef FEM_ENGINE_PREDICATES_H
#define FEM_ENGINE_PREDICATES_H

#include "geometry_2d.h"
#include <glm/glm.hpp>
#include <limits>
#include <cmath>
#include <glm/geometric.hpp>

namespace fem {

// Robust orientation predicate with epsilon handling
inline double orient2d(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    long double ax = a.x, ay = a.y;
    long double bx = b.x, by = b.y;
    long double cx = c.x, cy = c.y;
    long double det = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    return static_cast<double>(det);
}

inline int orient_sign(const glm::dvec2& a, const glm::dvec2& b, 
                      const glm::dvec2& c, double eps = 1e-15) {
    double v = orient2d(a, b, c);
    if (v > eps) return +1;
    if (v < -eps) return -1;
    return 0;
}

// Robust incircle predicate
inline double incircle_val(const glm::dvec2& a, const glm::dvec2& b, 
                          const glm::dvec2& c, const glm::dvec2& d) {
    // long double ax = a.x, ay = a.y, a2 = ax*ax + ay*ay;
    // long double bx = b.x, by = b.y, b2 = bx*bx + by*by;
    // long double cx = c.x, cy = c.y, c2 = cx*cx + cy*cy;
    // long double dx = d.x, dy = d.y, d2 = dx*dx + dy*dy;
    
    // long double det = 
    //     (ax*(by*(c2-d2) - cy*(b2-d2) + dy*(b2-c2))
    //     - ay*(bx*(c2-d2) - cx*(b2-d2) + dx*(b2-c2))
    //     + a2*(bx*(cy-dy) - cx*(by-dy) + dx*(by-cy))
    //     - (bx*cy*d2 - bx*dy*c2 - cx*by*d2 + cx*dy*b2 + dx*by*c2 - dx*cy*b2));
    
    // return static_cast<double>(det);
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

inline int inCircle_ccw_scaled_strict(const glm::dvec2& a,
                                     const glm::dvec2& b,
                                     const glm::dvec2& c,
                                     const glm::dvec2& d)
{
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

inline int inCircle_ccw(const glm::dvec2& a,
                        const glm::dvec2& b,
                        const glm::dvec2& c,
                        const glm::dvec2& d,
                        double /*eps*/ = 0.0)
{
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


static inline glm::dvec2 circumcenter(const glm::dvec2& A,
                                      const glm::dvec2& B,
                                      const glm::dvec2& C) {
    glm::dvec2 a = B - A, b = C - A;
    double aa = glm::dot(a,a), bb = glm::dot(b,b);
    double d  = 2.0 * (a.x*b.y - a.y*b.x);
    if (std::abs(d) < 1e-20) return Geometry2D::tri_centroid(A, B, C); // fallback
    glm::dvec2 u((bb*a.y - aa*b.y)/d, (aa*b.x - bb*a.x)/d);
    return A + u;
}

static inline glm::dvec2 incenter(const glm::dvec2& A,
                                  const glm::dvec2& B,
                                  const glm::dvec2& C)
{
    double a = glm::length(B - C);
    double b = glm::length(A - C);
    double c = glm::length(A - B);
    double s = a + b + c;
    return (a*A + b*B + c*C) / std::max(1e-30, s);
}

static inline bool is_obtuse(const glm::dvec2& A,
                             const glm::dvec2& B,
                             const glm::dvec2& C)
{
    auto obt = [](const glm::dvec2& p, const glm::dvec2& q, const glm::dvec2& r){
        return glm::dot(p - q, r - q) < 0.0; // obtuse at q if (p-q)·(r-q) < 0
    };
    return obt(A,B,C) || obt(B,C,A) || obt(C,A,B);
}

// move "t" toward incenter (0=cc, 1=ic)
static inline glm::dvec2 offcenter(const glm::dvec2& cc,
                                   const glm::dvec2& ic,
                                   double t = 0.35)   // 0.3–0.5 works well
{
    return (1.0 - t)*cc + t*ic;
}

}

#endif // FEM_ENGINE_PREDICATES_H