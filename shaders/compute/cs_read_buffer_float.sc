#include <bgfx_compute.sh>

BUFFER_RO(dataSrc, float, 0);
IMAGE2D_RW(dstTexture, r32f, 2);

uniform vec4 u_readBufferParams;

NUM_THREADS(8, 8, 1)
void main() {
    vec2 size = u_readBufferParams.xy;
    uint count = u_readBufferParams.z;

    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

    int width = int(size.x);
    int index = coord.y * width + coord.x;

    if (index >= count) {
        return;
    }

    float value = dataSrc[index];

    imageStore(dstTexture, coord, value);
}