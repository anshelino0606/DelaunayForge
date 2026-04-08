#include "output_quad.h"

#if defined(USE_BGFX)

namespace fem {

struct PosTexCoord0Vertex
{
    float x, y, z;
    float u, v;
};

static PosTexCoord0Vertex SCREEN_QUAD_VERTICES[] =
{
    // x, y, z,   u, v
    { -1.0f, -1.0f, 0.0f,  0.0f, 0.0f },
    {  1.0f, -1.0f, 0.0f,  1.0f, 0.0f },
    { -1.0f,  1.0f, 0.0f,  0.0f, 1.0f },
    {  1.0f,  1.0f, 0.0f,  1.0f, 1.0f },
};

static const uint16_t SCREEN_QUAD_INDICES[] =
{
    0, 1, 2,
    2, 1, 3, 
};

void OutputQuad::init() {
    destroy();

    bgfx::VertexLayout layout;

    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    StaticVertexBufferCreateInfo vb_create_info;
    vb_create_info.vertex_layout = &layout;
    vb_create_info.init_memory = bgfx::makeRef(SCREEN_QUAD_VERTICES, sizeof(PosTexCoord0Vertex) * 4);

    vb_.init(vb_create_info);

    StaticIndexBufferCreateInfo ib_create_info;
    ib_create_info.index_type = BufferIndexType::UINT16;
    ib_create_info.init_memory = bgfx::makeRef(SCREEN_QUAD_INDICES, sizeof(uint16_t) * 6);

    ib_.init(ib_create_info);
}

void OutputQuad::destroy() {
    vb_.destroy();
    ib_.destroy();
}

void OutputQuad::activate(uint8_t vertex_stream) {
    vb_.activate(vertex_stream);
    ib_.activate();
}
    
}

#endif // USE_BGFX