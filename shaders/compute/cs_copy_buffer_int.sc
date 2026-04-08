#include <bgfx_shader.sh>
#include <bgfx_compute.sh>

BUFFER_RO(pointsSrc, int, 0);
BUFFER_RW(pointsDst, int, 1);

uniform vec4 u_copyBuffersParams;

NUM_THREADS(64, 1, 1)
void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint count = u_copyBuffersParams.x;
    if (idx >= count) return;

    pointsDst[idx] = pointsSrc[idx];
}