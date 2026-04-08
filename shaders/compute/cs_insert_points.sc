// x = point_count
// y = base_idx     global point index of first point
// z = epsilon
// w = tri_count    current number of triangle slots to scan
uniform vec4 u_params;

#include "delaunay_common.sc"

#define MAX_BAD_TRIS   256u
#define MAX_CAVITY_EDGES (MAX_BAD_TRIS * 3u)

NUM_THREADS(64,1,1)
void main()
{
    uint pid_local = gl_GlobalInvocationID.x;
    uint point_count = uint(u_params.x);
    if (pid_local >= point_count) return;

    uint base       = uint(u_params.y);
    uint pid        = base + pid_local;

    uint tri_count  = loadPrevCounter(1u);
    float eps       = u_params.z;
    vec2  P         = points[pid].xy;

    uvec2 edges[MAX_CAVITY_EDGES];
    uint  edge_count = 0u;

    for (uint t = 0u; t < tri_count; ++t) {
        uvec4 T = triLoad(t);
        if (T.w == 0u) continue; // invalid

        vec2 A = points[T.x].xy;
        vec2 B = points[T.y].xy;
        vec2 C = points[T.z].xy;

        // Ensure CCW
        float o = orient(A, B, C);
        if (o < 0.0) {
            vec2 tmp = B; B = C; C = tmp;
        }

        float det = incircle_ccw(A, B, C, P);
        if (det > eps) {
            if (edge_count < MAX_CAVITY_EDGES) {
                edges[edge_count] = uvec2(packEdge(T.x, T.y), 0u);
                ++edge_count;
            }
            if (edge_count < MAX_CAVITY_EDGES) {
                edges[edge_count] = uvec2(packEdge(T.y, T.z), 0u);
                ++edge_count;
            }
            if (edge_count < MAX_CAVITY_EDGES) {
                edges[edge_count] = uvec2(packEdge(T.z, T.x), 0u);
                ++edge_count;
            }

            triStore(t, uvec4(0u, 0u, 0u, 0u));
        }
    }

    // point has a "cavity"
    if (edge_count > 0u) {
        // Count edge occurrences
        for (uint i = 0u; i < edge_count; ++i) {
            uint count = 0u;
            for (uint j = 0u; j < edge_count; ++j) {
                if (edges[i].x == edges[j].x) count++;
            }
            edges[i].y = count;
        }

        uint boundary_count = 0u;
        for (uint i = 0u; i < edge_count; ++i) {
            if (edges[i].y == 1u) {
                boundary_count++;
            }
        }

        if (boundary_count == 0u) {
            // Degenerate case
            return;
        }

        // allocate space for new triangles: triangles.push_back for each boundary edge
        uint new_tri_base = atomicAddCounter(1u, boundary_count);

        // Retriangulate cavity new triangles (P, a, b) for each boundary edge, with CCW orientation
        uint new_tri_idx = 0u;
        for (uint i = 0u; i < edge_count; ++i) {
            if (edges[i].y != 1u) continue; // interior edge

            uint packed = edges[i].x;
            uint a      = unpackA(packed);
            uint b      = unpackB(packed);

            vec2 vP = P;
            vec2 vA = points[a].xy;
            vec2 vB = points[b].xy;

            uint t_id = new_tri_base + new_tri_idx;
            new_tri_idx++;

            if (orient(vP, vA, vB) > 0.0) {
                triStore(t_id, uvec4(pid, a, b, 1u));
            } else {
                triStore(t_id, uvec4(pid, b, a, 1u));
            }
        }

        return;
    }

    //  no bad triangles > point is inside some triangle or outside hull

    // try to find containing triangle and split into 3
    for (uint t = 0u; t < tri_count; ++t) {
        uvec4 T = triLoad(t);
        if (T.w == 0u) continue;

        vec2 A = points[T.x].xy;
        vec2 B = points[T.y].xy;
        vec2 C = points[T.z].xy;

        float o0 = orient(A, B, P);
        float o1 = orient(B, C, P);
        float o2 = orient(C, A, P);

        if (o0 >= eps && o1 >= eps && o2 >= eps) {
            uint base_tri = atomicAddCounter(1u, 3u);

            triStore(t, uvec4(0u, 0u, 0u, 0u));

            triStore(base_tri + 0u, uvec4(pid, T.x, T.y, 1u));
            triStore(base_tri + 1u, uvec4(pid, T.y, T.z, 1u));
            triStore(base_tri + 2u, uvec4(pid, T.z, T.x, 1u));

            return;
        }
    }

    // outside convex hull or fully degenerate > ignore for now – TODO: later
    return;
}
