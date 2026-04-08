#include <bgfx_shader.sh>
#include <bgfx_compute.sh>

BUFFER_RW(counters, uvec4, 0);

NUM_THREADS(64, 1, 1)
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= 23) return;

    uint value = (idx * 2) + 1;

    uint orig;
    counters[idx] = uvec4(value);
    //InterlockedAdd(counters[0], value, orig);   // HLSL needs the out param
}