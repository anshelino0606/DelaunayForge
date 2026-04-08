#include <bgfx_shader.sh>
#include <bgfx_compute.sh>

BUFFER_RO(triangles, uvec4, 0);   // xyz = vertex ids, w = valid
BUFFER_RO(badMask,   uint,  1);   // 1 = bad triangle

BUFFER_RW(edgeCount, uint,  2);

uniform vec4 u_params;

uint hashEdge(uint a, uint b)
{
    uint mn = min(a,b), mx = max(a,b);
    uint h  = (mn * 73856093u) ^ (mx * 19349663u);
    h ^= h >> 16; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return h;
}

void atomicAddEdge(uint idx, uint v)
{
#if BGFX_SHADER_LANGUAGE_HLSL
    InterlockedAdd(edgeCount[idx], v);
#else
    atomicAdd(edgeCount[idx], v);
#endif
}

void bumpEdge(uint a, uint b, uint tableW)
{
    uint idx = hashEdge(a,b) % max(1u, tableW);
    atomicAddEdge(idx, 1u);
}

NUM_THREADS(64, 1, 1)
void main()
{
    uint tid      = gl_GlobalInvocationID.x;
    const uint N  = uint(u_params.x);
    const uint W  = uint(u_params.y);

    if (tid >= N)                   return;
    if (badMask[tid] == 0u)         return;

    uvec4 tri = triangles[tid];
    if (tri.w == 0u)                return;

    uint v0 = tri.x, v1 = tri.y, v2 = tri.z;

    bumpEdge(min(v0,v1), max(v0,v1), W);
    bumpEdge(min(v1,v2), max(v1,v2), W);
    bumpEdge(min(v2,v0), max(v2,v0), W);
}
