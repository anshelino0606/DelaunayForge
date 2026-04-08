#include <bgfx_compute.sh>

BUFFER_RO(pointsBuffer,    vec4, 0);  // xyz = position, w = boundary
BUFFER_RO(trianglesBuffer, uvec4, 1);
BUFFER_RW(outputBuffer, vec4, 2);

uniform vec4 u_params; // x=point_count, y=tri_count, z=eps, w=unused

float orient2d(vec2 a, vec2 b, vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

float computeAngle(vec2 a, vec2 b, vec2 c) {
    vec2 ab = b - a;
    vec2 ac = c - a;
    float dot_prod = dot(ab, ac);
    float denom = length(ab) * length(ac);
    if (denom < 1e-10) return 0.0;
    float cang = clamp(dot_prod / denom, -1.0, 1.0);
    return acos(cang) * 180.0 / 3.14159265359;
}

NUM_THREADS(64, 1, 1)
void main()
{
    uint tid = gl_GlobalInvocationID.x;
    uint tri_count = uint(u_params.y);
    if (tid >= tri_count) return;

    uvec4 tri = trianglesBuffer[tid];
    if (tri.w == 0u) {
        outputBuffer[tid] = vec4(0.0);
        return;
    }

    vec2 p0 = pointsBuffer[tri.x].xy;
    vec2 p1 = pointsBuffer[tri.y].xy;
    vec2 p2 = pointsBuffer[tri.z].xy;

    float a0 = computeAngle(p0,p1,p2);
    float a1 = computeAngle(p1,p2,p0);
    float a2 = computeAngle(p2,p0,p1);

    float min_angle = min(a0, min(a1, a2));
    float area = abs(orient2d(p0,p1,p2)) * 0.5;

    float e0=length(p1-p0), e1=length(p2-p1), e2=length(p0-p2);
    float max_edge = max(e0, max(e1, e2));
    float s = 0.5*(e0+e1+e2);
    float quality = (area > 1e-10) ? (area / (s * max_edge)) : 0.0;
    float avg_angle = (a0+a1+a2)/3.0;

    outputBuffer[tid] = vec4(min_angle, avg_angle, quality, area);
}
