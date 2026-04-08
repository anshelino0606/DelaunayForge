#include "delaunay_common.sc"

// u_params.x = numTris
// u_params.y = total loop vertices (for info only)
// u_params.z = numLoops
uniform vec4 u_params;

// Bindings must match setBuffer() calls in C++.
// points[] and triLoad/triStore() come from delaunay_common.sc (bindings 0 and 1).

// Correct order is (name, type, binding)
BUFFER_RO(L, vec4,   2); // loop vertices: (x,y,0,0)
BUFFER_RO(M, ivec4,  3); // per-loop meta: (start,count,is_outer,0)

bool pointInPoly(vec2 p, uint start, uint count)
{
    bool inside = false;
    for (uint i = 0u, j = count - 1u; i < count; j = i, ++i) {
        vec2 A = L[start + i].xy;
        vec2 B = L[start + j].xy;
        bool inter = ((A.y > p.y) != (B.y > p.y)) &&
                     (p.x < (B.x - A.x) * (p.y - A.y) / max(B.y - A.y, 1e-20) + A.x);
        if (inter) inside = !inside;
    }
    return inside;
}

bool segSeg(vec2 a, vec2 b, vec2 c, vec2 d)
{
    vec2 r = b - a, s = d - c;
    float rxs  = r.x * s.y - r.y * s.x;
    float qpxr = (c.x - a.x) * r.y - (c.y - a.y) * r.x;

    // Collinear / parallel => not treated as blocking here.
    if (abs(rxs) < 1e-20 && abs(qpxr) < 1e-20) return false;
    if (abs(rxs) < 1e-20)                      return false;

    float t = ((c.x - a.x) * s.y - (c.y - a.y) * s.x) / rxs;
    float u = ((c.x - a.x) * r.y - (c.y - a.y) * r.x) / rxs;
    return (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0);
}

NUM_THREADS(64, 1, 1)
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(u_params.x)) return;

    uvec4 tri = triLoad(i);
    if (tri.w == 0u) return;

    vec2 a = points[tri.x].xy;
    vec2 b = points[tri.y].xy;
    vec2 c = points[tri.z].xy;
    vec2 C = (a + b + c) * (1.0/3.0);

    bool keep = true;

    // Inside at least one OUTER loop (M[k].z == 0)
    bool insideOuter = false;
    for (uint k = 0u; k < uint(u_params.z); ++k) {
        ivec4 mk = M[k];
        if (mk.z == 0 && pointInPoly(C, uint(mk.x), uint(mk.y))) {
            insideOuter = true;
            break;
        }
    }
    if (!insideOuter) keep = false;

    // Not inside any HOLE (M[k].z != 0)
    for (uint k = 0u; keep && k < uint(u_params.z); ++k) {
        ivec4 mk = M[k];
        if (mk.z == 1 && pointInPoly(C, uint(mk.x), uint(mk.y))) { 
            keep = false;
        }
    }

#ifdef CHECK_INTERSECTIONS
    // Optional: reject triangles that cross any loop edge.
    if (keep) {
        for (uint k = 0u; keep && k < uint(u_params.z); ++k) {
            ivec4 mk = M[k];
            uint start = uint(mk.x), count = uint(mk.y);
            for (uint e = 0u, f = count - 1u; e < count; f = e++) {
                vec2 c0 = L[start + e].xy;
                vec2 c1 = L[start + f].xy;
                if (segSeg(a, b, c0, c1) || segSeg(b, c, c0, c1) || segSeg(c, a, c0, c1)) {
                    keep = false; break;
                }
            }
        }
    }
#endif

    if (!keep) {
        tri.w = 0u;
        triStore(i, tri);
    }
}
