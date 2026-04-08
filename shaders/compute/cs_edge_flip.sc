#include <bgfx_compute.sh>

BUFFER_RO(pointsBuffer,     vec4,  0);
BUFFER_RO(trianglesBuffer,  uvec4, 1);
BUFFER_RO(neighborsBuffer,  ivec4, 2);

IMAGE2D_RW(flipImage, r32ui, 0);

uniform vec4 u_params; // x=tri_count, y=epsilon

float computeAngle(vec2 a, vec2 b, vec2 c) {
    vec2 ab = b - a;
    vec2 ac = c - a;
    float denom = length(ab) * length(ac);
    if (denom < 1e-10) return 0.0;
    float cang = clamp(dot(ab,ac)/denom, -1.0, 1.0);
    return acos(cang) * 180.0 / 3.14159265359;
}

float triangleMinAngle(vec2 a, vec2 b, vec2 c) {
    return min(computeAngle(a,b,c), min(computeAngle(b,c,a), computeAngle(c,a,b)));
}

NUM_THREADS(64,1,1)
void main()
{
    uint tid = gl_GlobalInvocationID.x;
    uint tri_count = uint(u_params.x);
    if (tid >= tri_count) return;

    uvec4 tri = trianglesBuffer[tid];
    if (tri.w == 0u) {
        imageStore(flipImage, ivec2(int(tid),0), uvec4(0u));
        return;
    }

    ivec4 neighbors = neighborsBuffer[tid];
    bool should_flip = false;

    for (int edge=0; edge<3 && !should_flip; ++edge) {
        int nb = neighbors[edge];
        if (nb < 0 || nb >= int(tri_count)) continue;

        uvec4 ntri = trianglesBuffer[nb];
        if (ntri.w == 0u) continue;

        uint va, vb, v_this, v_other;
        if (edge==0){ va=tri.x; vb=tri.y; v_this=tri.z; }
        else if (edge==1){ va=tri.y; vb=tri.z; v_this=tri.x; }
        else { va=tri.z; vb=tri.x; v_this=tri.y; }

        v_other = (ntri.x!=va && ntri.x!=vb) ? ntri.x :
                  (ntri.y!=va && ntri.y!=vb) ? ntri.y : ntri.z;

        vec2 pa = pointsBuffer[va].xy;
        vec2 pb = pointsBuffer[vb].xy;
        vec2 p_this  = pointsBuffer[v_this].xy;
        vec2 p_other = pointsBuffer[v_other].xy;

        float curMin = min(triangleMinAngle(pa,pb,p_this), triangleMinAngle(pa,pb,p_other));
        float flpMin = min(triangleMinAngle(p_this,p_other,pa), triangleMinAngle(p_this,p_other,pb));

        if (flpMin > curMin + 1e-6) should_flip = true;
    }

    imageStore(flipImage, ivec2(int(tid),0), uvec4(should_flip ? 1u : 0u));
}
