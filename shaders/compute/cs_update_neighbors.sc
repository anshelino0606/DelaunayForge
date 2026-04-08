// cs_update_neighbors.sc
#include "delaunay_common.sc"

uniform vec4 u_params; // .x = triCount

NUM_THREADS(64,1,1)
void main() {
    uint i = gl_GlobalInvocationID.x;
    uint triCount = uint(u_params.x + 0.5);
    if (i >= triCount) return;

    uvec4 Ti = triLoad(i);
    if (Ti.w == 0u) {
        neighborStore(i, ivec4(-1, -1, -1, -1));
        return;
    }

    uint a = Ti.x;
    uint b = Ti.y;
    uint c = Ti.z;

    ivec4 N = ivec4(-1, -1, -1, -1);

    for (uint j = 0u; j < triCount; ++j) {
        if (j == i) continue;

        uvec4 Tj = triLoad(j);
        if (Tj.w == 0u) continue;

        uint d = Tj.x;
        uint e = Tj.y;
        uint f = Tj.z;

        bool has_a = (d == a || e == a || f == a);
        bool has_b = (d == b || e == b || f == b);
        bool has_c = (d == c || e == c || f == c);

        // edge (a,b)
        if (has_a && has_b && N.x == -1) {
            N.x = int(j);
        }

        // edge (b,c)
        if (has_b && has_c && N.y == -1) {
            N.y = int(j);
        }

        // edge (c,a)
        if (has_c && has_a && N.z == -1) {
            N.z = int(j);
        }
    }

    neighborStore(i, N);
}
