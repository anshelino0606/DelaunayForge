#ifndef FEM_OUTPUT_QUAD_H
#define FEM_OUTPUT_QUAD_H

#if defined(USE_BGFX)

#include "buffer.h"

namespace fem {

class OutputQuad {
public:
    void init();
    void destroy();

    void activate(uint8_t vertex_stream = 0);

private:
    StaticVertexBuffer vb_;
    StaticIndexBuffer ib_;
};

}

#endif // USE_BGFX

#endif // FEM_OUTPUT_QUAD_H