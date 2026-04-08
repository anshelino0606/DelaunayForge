#include <bgfx_shader.sh>
#include <bgfx_compute.sh>

#define BIND_POINTS      0
#define BIND_TRIS        1
#define BIND_NEIGHBORS   2
#define BIND_SEEDTRI     3
#define BIND_PSTATUS     4
#define BIND_BOUNDARY    5
#define BIND_EDGEARENA   6
#define BIND_OWNER       8
#define BIND_COUNTERS    9
#define BIND_PREV_COUNTERS 10

#define IMG_COUNTERS     0
#define IMG_TRI_OUT      1
#define IMG_VEC4_OUT     2

BUFFER_RO (points,    vec4,  BIND_POINTS);
BUFFER_RW (triangles, uvec4, BIND_TRIS);
BUFFER_RW (neighbors, ivec4, BIND_NEIGHBORS);

#ifndef COMMON_STEINER_MODE
#  define COMMON_STEINER_MODE 0
#endif

#if COMMON_STEINER_MODE
    BUFFER_RW (steinerCandidates, vec4, BIND_SEEDTRI);
    void steinerStore(uint i, vec4 v) { steinerCandidates[i] = v; }
#else
    BUFFER_RW (seedtri,   ivec4, BIND_SEEDTRI);
    ivec4 seedLoad(uint i)           { return seedtri[i]; }
    void  seedStore(uint i, ivec4 v) { seedtri[i] = v; }
#endif

BUFFER_RW (pstatus,   uvec4, BIND_PSTATUS);
BUFFER_RW (edgeArena, uvec4, BIND_EDGEARENA);
BUFFER_RW (ownerBuf,  ivec4,   BIND_OWNER);
BUFFER_RW (countersBuf, uint, BIND_COUNTERS);
BUFFER_RW (prevCountersBuf, uint, BIND_PREV_COUNTERS);

uvec4 triLoad(uint i)                { return triangles[i]; }
void  triStore(uint i, uvec4 v)      { triangles[i] = v; }

ivec4 neighborLoad(uint i)           { return neighbors[i]; }
void  neighborStore(uint i, ivec4 v) { neighbors[i] = v; }

vec2 pointLoad(uint i)
{
    return points[i].xy;
}

uvec4 pstatusLoad(uint i)            { return pstatus[i]; }
void  pstatusStore(uint i, uvec4 v)  { pstatus[i] = v; }

uint loadCounter(uint idx)
{
    return countersBuf[idx];
}

uint loadPrevCounter(uint idx)
{
    return prevCountersBuf[idx];
}

uint atomicAddCounter(uint idx, uint val)
{
    uint orig;
    InterlockedAdd(countersBuf[idx], val, orig);  // HLSL: dest, value, out original
    return orig;
}

/*
void atomicExchangeCounter(uint idx, uint val)
{
    uint dummy;
    InterlockedExchange(countersBuf[idx], val, dummy);
}
*/

void setProgress()
{
    uint _;
    InterlockedMax(countersBuf[2], 1u, _);   // progress = max(progress, 1)
}

static const int OWNER_UNCLAIMED = -1;

int  loadOwner(uint triIdx)             { return ownerBuf[triIdx].x; }
void storeOwner(uint triIdx, int value) { ownerBuf[triIdx].x = value; }

int atomicCASOwner(uint triIdx, int expected, int desired)
{
    int orig;
    InterlockedCompareExchange(ownerBuf[triIdx].x, expected, desired, orig);
    return orig;
}

uint packEdge(uint a, uint b)
{
    uint mn = min(a, b);
    uint mx = max(a, b);
    return (mn & 0xFFFFu) | ((mx & 0xFFFFu) << 16);
}

uint unpackA(uint p){ return  p        & 0xFFFFu; }
uint unpackB(uint p){ return (p >> 16) & 0xFFFFu; }

float orient(vec2 a, vec2 b, vec2 c)
{
    return (b.x - a.x) * (c.y - a.y)
         - (b.y - a.y) * (c.x - a.x);
}

// >0 : P is inside circumcircle, 0 : on, <0 : outside.
float incircle_ccw(vec2 A, vec2 B, vec2 C, vec2 P)
{
    float ax = A.x - P.x;
    float ay = A.y - P.y;
    float bx = B.x - P.x;
    float by = B.y - P.y;
    float cx = C.x - P.x;
    float cy = C.y - P.y;

    float a2 = ax*ax + ay*ay;
    float b2 = bx*bx + by*by;
    float c2 = cx*cx + cy*cy;

    float det =
          ax*(by*c2 - b2*cy)
        - ay*(bx*c2 - b2*cx)
        + a2*(bx*cy - by*cx);

    return det;
}
