// cs_update_topology.sc
#include "delaunay_common.sc"

uniform vec4 u_params; // x=tri_count, y=enable_flipping, z=epsilon

NUM_THREADS(64,1,1)
void main()
{
    uint tid = gl_GlobalInvocationID.x;
    uint tri_count = uint(u_params.x);
    
    if (tid >= tri_count) return;
    
    // Update neighbors for this triangle
    
    uvec4 T = triLoad(tid);
    if (T.w == 0u) {
        neighborStore(tid, ivec4(-1, -1, -1, -1));
        return;
    }
    
    uint e0 = packEdge(T.x, T.y);
    uint e1 = packEdge(T.y, T.z);
    uint e2 = packEdge(T.z, T.x);
    
    int n0 = -1, n1 = -1, n2 = -1;
    
    // Search all triangles for matching edges
    for (uint other = 0u; other < tri_count; ++other) {
        if (other == tid) continue;
        
        uvec4 T2 = triLoad(other);
        if (T2.w == 0u) continue;
        
        uint o0 = packEdge(T2.x, T2.y);
        uint o1 = packEdge(T2.y, T2.z);
        uint o2 = packEdge(T2.z, T2.x);
        
        if (e0 == o0 || e0 == o1 || e0 == o2) n0 = int(other);
        if (e1 == o0 || e1 == o1 || e1 == o2) n1 = int(other);
        if (e2 == o0 || e2 == o1 || e2 == o2) n2 = int(other);
    }
    
    neighborStore(tid, ivec4(n0, n1, n2, -1));
    
    bool enable_flipping = (u_params.y > 0.5);
    if (!enable_flipping) return;
    
    float eps = u_params.z;
    ivec4 N = ivec4(n0, n1, n2, -1);
    
    for (uint edge_idx = 0u; edge_idx < 3u; ++edge_idx) {
        int nbr = N[edge_idx];
        if (nbr < 0 || nbr >= int(tri_count)) continue;
        
        uvec4 T_nbr = triLoad(uint(nbr));
        if (T_nbr.w == 0u) continue;
        
        // Only flip once per edge
        if (uint(nbr) < tid) continue;
        
        // Get edge vertices
        uint a, b, c;
        if (edge_idx == 0u) {
            a = T.x; b = T.y; c = T.z;
        } else if (edge_idx == 1u) {
            a = T.y; b = T.z; c = T.x;
        } else {
            a = T.z; b = T.x; c = T.y;
        }
        
        // Find opposite vertex in neighbor
        uint d = 0xFFFFFFFFu;
        if (T_nbr.x != a && T_nbr.x != b) d = T_nbr.x;
        else if (T_nbr.y != a && T_nbr.y != b) d = T_nbr.y;
        else if (T_nbr.z != a && T_nbr.z != b) d = T_nbr.z;
        
        if (d == 0xFFFFFFFFu) continue;
        
        // Check Delaunay criterion
        vec2 vA = points[a].xy;
        vec2 vB = points[b].xy;
        vec2 vC = points[c].xy;
        vec2 vD = points[d].xy;
        
        // Ensure CCW
        float o = orient(vA, vB, vC);
        if (o < 0.0) {
            vec2 tmp = vB; vB = vC; vC = tmp;
        }
        
        float det = incircle_ccw(vA, vB, vC, vD);
        
        // If D is inside circumcircle, flip edge
        if (det > eps) {
            // New triangles: (c,d,a) and (c,d,b)
            if (orient(vC, vD, vA) > 0.0) {
                triStore(tid, uvec4(c, d, a, 1u));
            } else {
                triStore(tid, uvec4(c, a, d, 1u));
            }
            
            if (orient(vC, vD, vB) > 0.0) {
                triStore(uint(nbr), uvec4(c, d, b, 1u));
            } else {
                triStore(uint(nbr), uvec4(c, b, d, 1u));
            }
            
            setProgress();
        }
    }
}