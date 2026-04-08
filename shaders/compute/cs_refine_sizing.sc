// cs_refine_sizing.sc

#define COMMON_STEINER_MODE 1
#include "delaunay_common.sc"

BUFFER_RO(boundaryPolyline, vec4, 5);  // Each vec4 = (x, y, 0, 0)


uniform vec4 u_params;      // [triCount, threshold, max_steiner, global_h]
uniform vec4 u_density0;    // [boundary_h_min, boundary_h_max, boundary_influence, boundary_count]
uniform vec4 u_density1;    // [radial_cx, radial_cy, radial_r_in, radial_r_out]
uniform vec4 u_density2;    // [radial_h_min, radial_h_max, use_boundary, use_radial]

float distToSegment(vec2 P, vec2 A, vec2 B) {
    vec2 AB = B - A;
    vec2 AP = P - A;
    float t = clamp(dot(AP, AB) / max(1e-12, dot(AB, AB)), 0.0, 1.0);
    vec2 closest = A + t * AB;
    return length(P - closest);
}

float evaluateDensity(vec2 pos) {
    float h = u_params.w;  // Start with global_h
    
    // Boundary density
    if (u_density2.z > 0.5) {
        uint boundary_count = uint(u_density0.w);
        float min_dist = 1e10;
        
        for (uint i = 0; i < boundary_count; ++i) {
            vec2 A = boundaryPolyline[i].xy;
            vec2 B = boundaryPolyline[(i + 1u) % boundary_count].xy;
            float d = distToSegment(pos, A, B);
            min_dist = min(min_dist, d);
        }
        
        float influence = u_density0.z;
        if (min_dist < influence) {
            float t = min_dist / influence;
            float h_boundary = u_density0.x + t * (u_density0.y - u_density0.x);
            h = min(h, h_boundary);
        }
    }
    
    // Radial density
    if (u_density2.w > 0.5) {
        vec2 center = u_density1.xy;
        float r_in = u_density1.z;
        float r_out = u_density1.w;
        float dist = length(pos - center);
        
        if (dist <= r_in) {
            h = min(h, u_density2.x);  // radial_h_min
        } else if (dist < r_out) {
            float t = (dist - r_in) / (r_out - r_in);
            float h_radial = u_density2.x + t * (u_density2.y - u_density2.x);
            h = min(h, h_radial);
        }
    }
    
    return h;
}

NUM_THREADS(64, 1, 1)
void main() {
    uint  tri_id      = gl_GlobalInvocationID.x;
    uint  triCount    = uint(u_params.x);
    float threshold   = u_params.y;
    uint  max_steiner = uint(u_params.z);
    if (tri_id >= triCount) return;

    uvec4 tri = triLoad(tri_id);
    if (tri.w == 0u) return;

    ivec4 neigh = neighborLoad(tri_id);

    // process each edge once
    for (int i = 0; i < 3; ++i) {
        int n = neigh[i];
        if (n != -1 && uint(n) < tri_id) continue;

        uint v0 = tri[i];
        uint v1 = tri[(i+1)%3];

        vec2 p0 = points[v0].xy;
        vec2 p1 = points[v1].xy;
        vec2 mid = 0.5*(p0+p1);
        float L = length(p1 - p0);

        float h = evaluateDensity(mid);

        if (L > h * threshold) {
            // write to counter slot 3 (matches your C++)
            uint idx = atomicAddCounter(3u, 1u);
            if (idx < max_steiner) {
                steinerStore(idx, vec4(mid, L/h, 1.0));
            }
        }
    }
}