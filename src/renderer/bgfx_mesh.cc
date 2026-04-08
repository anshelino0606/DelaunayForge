#ifdef USE_BGFX
#include "bgfx_mesh.h"
#include <cstring>
#include <algorithm>


bgfx::VertexLayout BgfxMesh::s_layout;

BgfxMesh::BgfxMesh() {
    if (s_layout.getStride() == 0) {
        s_layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .end();
    }
    u_color_ = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
}

BgfxMesh::~BgfxMesh() { 
    destroy(); 
}

void BgfxMesh::destroy() {
    if (bgfx::isValid(vbh_)) bgfx::destroy(vbh_);
    if (bgfx::isValid(ibh_)) bgfx::destroy(ibh_);
    if (bgfx::isValid(u_color_)) { 
        bgfx::destroy(u_color_); 
        u_color_ = BGFX_INVALID_HANDLE; 
    }
    vbh_ = BGFX_INVALID_HANDLE; 
    ibh_ = BGFX_INVALID_HANDLE;
    vcap_ = icap_ = 0;
}

void BgfxMesh::update(const std::vector<glm::vec3>& verts,
                      const std::vector<unsigned int>& indices)
{
    const uint32_t vcount = (uint32_t)verts.size();
    const uint32_t icount = (uint32_t)indices.size();
    
    // Create or resize vertex buffer if needed
    if (!bgfx::isValid(vbh_) || vcount > vcap_) {
        if (bgfx::isValid(vbh_)) bgfx::destroy(vbh_);
        vcap_ = std::max(vcount, 1u);
        vbh_ = bgfx::createDynamicVertexBuffer(vcap_, s_layout, BGFX_BUFFER_ALLOW_RESIZE);
    }
    
    // Create or resize index buffer if needed
    if (!bgfx::isValid(ibh_) || icount > icap_) {
        if (bgfx::isValid(ibh_)) bgfx::destroy(ibh_);
        icap_ = std::max(icount, 3u);
        ibh_ = bgfx::createDynamicIndexBuffer(icap_, BGFX_BUFFER_INDEX32 | BGFX_BUFFER_ALLOW_RESIZE);
    }
    
    // Update vertex buffer data (ONLY ONCE)
    if (vcount > 0) {
        const bgfx::Memory* vm = bgfx::alloc(vcount * sizeof(PosVertex));
        auto* out = (PosVertex*)vm->data;
        for (uint32_t i = 0; i < vcount; ++i) {
            out[i].x = verts[i].x; 
            out[i].y = verts[i].y; 
            out[i].z = verts[i].z;
        }
        bgfx::update(vbh_, 0, vm);
    }
    
    // Update index buffer data (ONLY ONCE)
    if (icount > 0) {
        const bgfx::Memory* im = bgfx::copy(indices.data(), icount * sizeof(uint32_t));
        bgfx::update(ibh_, 0, im);
    }
    
    vcount_ = vcount;
    icount_ = icount;
}

void BgfxMesh::submit(bgfx::ProgramHandle prog, uint16_t viewId, uint64_t state) const
{
    if (!bgfx::isValid(vbh_) || !bgfx::isValid(ibh_) || !bgfx::isValid(prog)) return;
    if (vcount_ == 0 || icount_ == 0) return;
    
    uint64_t st = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                  BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                  BGFX_STATE_MSAA;
    st |= state;
    
    bgfx::setState(st);
    bgfx::setVertexBuffer(0, vbh_, 0, vcount_);
    bgfx::setIndexBuffer(ibh_, 0, icount_);
    bgfx::setUniform(u_color_, color_);
    bgfx::submit(viewId, prog);
}

#endif // USE_BGFX