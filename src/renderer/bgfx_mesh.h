#pragma once
#ifdef USE_BGFX

#include <bgfx/bgfx.h>
#include <glm/vec3.hpp>
#include <vector>

class BgfxMesh {
public:
    BgfxMesh();
    ~BgfxMesh();

    void update(const std::vector<glm::vec3>& verts,
                const std::vector<unsigned int>& indices);

    void submit(bgfx::ProgramHandle prog, uint16_t viewId = 0, uint64_t state = 0) const;

    void destroy();

    void setColor(float r, float g, float b, float a) {
        color_[0] = r; color_[1] = g; color_[2] = b; color_[3] = a;
    }

private:
    struct PosVertex { float x, y, z; };
    static bgfx::VertexLayout s_layout;

    bgfx::DynamicVertexBufferHandle vbh_ = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle  ibh_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle             u_color_ = BGFX_INVALID_HANDLE;

    uint32_t vcap_ = 0;
    uint32_t icap_ = 0;

    uint32_t vcount_ = 0;
    uint32_t icount_ = 0;

    float color_[4] = {0.95f, 0.95f, 0.98f, 1.0f};
};

#endif
