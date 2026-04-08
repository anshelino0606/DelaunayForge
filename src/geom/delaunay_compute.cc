#ifdef USE_BGFX

#include "delaunay_compute.h"
#include "rhi/buffer.h"
#include "rhi/storage_buffer.h"
#include <cstdio>
#include <cstring>
#include <string>
#include "log_categories.h"

namespace fem {

DelaunayComputeManager::DelaunayComputeManager() {}

DelaunayComputeManager::~DelaunayComputeManager() {
    shutdown();
}

bool isComputeFormatSupported(bgfx::TextureFormat::Enum fmt) {
    const bgfx::Caps* caps = bgfx::getCaps();
    return (caps->supported & BGFX_CAPS_COMPUTE) != 0 &&
           (caps->formats[fmt] & BGFX_CAPS_FORMAT_TEXTURE_IMAGE_WRITE) != 0;
}

bool DelaunayComputeManager::init() {
    if (initialized_) return true;
    
    triangle_quality_program_.init("cs_delaunay.bin");
    edge_flip_program_.init("cs_edge_flip.bin");
    
    // Create uniforms
    u_params_ = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
    
    initialized_ = true;
    LOGT_DEBUG(LogGeometry, "Initialized successfully");
    return true;
}

void DelaunayComputeManager::shutdown() {
    if (!initialized_) return;
    
    triangle_quality_program_.destroy();
    edge_flip_program_.destroy();
    if (bgfx::isValid(u_params_)) bgfx::destroy(u_params_);
    
    u_params_ = BGFX_INVALID_HANDLE;
    
    initialized_ = false;
}

DelaunayComputeManager::ComputeResult 
DelaunayComputeManager::compute_triangle_metrics(const std::vector<Point2D>& points,
                                                 const std::vector<Tri>& triangles)
{
    ComputeResult result;
    if (!initialized_) {
        LOGT_ERROR(LogGeometry, "Not initialized!");
        return result;
    }
    
    if (triangles.empty()) {
        LOGT_WARN(LogGeometry, "No triangles to process");
        return result;
    }

    const uint32_t triCount = (uint32_t)triangles.size();

    std::vector<glm::vec4> P(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        P[i] = glm::vec4(points[i].x(), points[i].y(), 0.0f, points[i].on_boundary ? 1.0f : 0.0f);
    }

    std::vector<glm::uvec4> T(triCount);
    for (uint32_t i = 0; i < triCount; ++i) {
        T[i] = glm::uvec4(triangles[i].v[0], triangles[i].v[1], triangles[i].v[2], triangles[i].valid ? 1u : 0u);
    }

    const fem::BufferFlagBits flags = fem::FEM_BUFFER_FLAG_STAGING | fem::FEM_BUFFER_FLAG_RESIZABLE;
    
    const bgfx::Memory* pmem = bgfx::copy(P.data(), (uint32_t)(P.size() * sizeof(glm::vec4)));
    const bgfx::Memory* tmem = bgfx::copy(T.data(), (uint32_t)(T.size() * sizeof(glm::uvec4)));

    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float).end();

    fem::DynamicVertexBufferCreateInfo buf_info;
    buf_info.vertex_layout = &layout;
    buf_info.flags = flags;
    buf_info.init_memory = pmem;

    fem::DynamicVertexBuffer p_buf;
    p_buf.init(buf_info);

    buf_info.init_memory = tmem;

    fem::DynamicVertexBuffer t_buf;
    t_buf.init(buf_info);

    fem::StorageBuffer<glm::vec4> output_storage_buf;
    output_storage_buf.init(triCount);

    float params[4] = { (float)points.size(), (float)triCount, 1e-12f, 0.0f };
    bgfx::setUniform(u_params_, params);

    p_buf.bind(0, bgfx::Access::Read);
    t_buf.bind(1, bgfx::Access::Read);
    output_storage_buf.bind(2, bgfx::Access::ReadWrite);

    const uint32_t groups = (triCount + 63u) / 64u;
    triangle_quality_program_.dispatch(groups);

    // Resize result vectors
    result.min_angles.resize(triCount);
    result.avg_angles.resize(triCount);
    result.qualities.resize(triCount);
    result.areas.resize(triCount);

    std::vector<glm::vec4> out(triCount);
    output_storage_buf.read(out, triCount);
    
    for (uint32_t i = 0; i < triCount; ++i) {
        result.min_angles[i] = out[i].x;
        result.avg_angles[i] = out[i].y;
        result.qualities[i]  = out[i].z;
        result.areas[i]      = out[i].w;
    }

    p_buf.destroy();
    t_buf.destroy();
    output_storage_buf.destroy();

    return result;
}

std::vector<bool>
DelaunayComputeManager::compute_edge_flips(const std::vector<Point2D>& points,
                                           const std::vector<Tri>& triangles)
{
    std::vector<bool> flip(triangles.size(), false);
    // if (!initialized_ || triangles.empty()) return flip;

    // const uint32_t triCount = (uint32_t)triangles.size();
    
    // if (!bgfx::isValid(flipTex_) || flipWidth_ < triCount) {
    //     if (bgfx::isValid(flipTex_)) bgfx::destroy(flipTex_);
    //     flipTex_ = makeTexR32U(triCount);
    //     flipWidth_ = triCount;
    // }

    // std::vector<glm::vec4> P(points.size());
    // for (size_t i = 0; i < points.size(); ++i) {
    //     P[i] = glm::vec4(points[i].x, points[i].y, 0, points[i].on_boundary ? 1.0f : 0.0f);
    // }
    
    // std::vector<glm::uvec4> T(triangles.size());
    // std::vector<glm::ivec4> N(triangles.size());
    // for (size_t i = 0; i < triangles.size(); ++i) {
    //     T[i] = glm::uvec4(triangles[i].v[0], triangles[i].v[1], triangles[i].v[2], triangles[i].valid ? 1u : 0u);
    //     N[i] = glm::ivec4(triangles[i].neighbors[0], triangles[i].neighbors[1], triangles[i].neighbors[2], -1);
    // }

    // const uint64_t computeFlags = BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_ALLOW_RESIZE;
    // bgfx::VertexLayout layout;
    // layout.begin().add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float).end();

    // const bgfx::Memory* pmem = bgfx::copy(P.data(), (uint32_t)(P.size() * sizeof(glm::vec4)));
    // const bgfx::Memory* tmem = bgfx::copy(T.data(), (uint32_t)(T.size() * sizeof(glm::uvec4)));
    // const bgfx::Memory* nmem = bgfx::copy(N.data(), (uint32_t)(N.size() * sizeof(glm::ivec4)));

    // auto pBuf = bgfx::createDynamicVertexBuffer(pmem, layout, computeFlags);
    // auto tBuf = bgfx::createDynamicVertexBuffer(tmem, layout, computeFlags);
    // auto nBuf = bgfx::createDynamicVertexBuffer(nmem, layout, computeFlags);

    // float params[4] = { (float)triCount, 1e-12f, 0, 0 };
    // bgfx::setUniform(u_params_, params);

    // bgfx::setBuffer(0, pBuf, bgfx::Access::Read);
    // bgfx::setBuffer(1, tBuf, bgfx::Access::Read);
    // bgfx::setBuffer(2, nBuf, bgfx::Access::Read);
    // bgfx::setImage(0, flipTex_, 0, bgfx::Access::Write, bgfx::TextureFormat::R32U);

    // const uint32_t groups = (triCount + 63u) / 64u;
    // bgfx::dispatch(0, edge_flip_program_, groups, 1, 1);
    
    // // Submit and wait
    // bgfx::frame();
    // bgfx::frame();

    // std::vector<uint32_t> out(triCount);
    // bgfx::readTexture(flipTex_, out.data());
    
    // bgfx::frame();
    // bgfx::frame();

    // bgfx::destroy(pBuf);
    // bgfx::destroy(tBuf);
    // bgfx::destroy(nBuf);

    // for (uint32_t i = 0; i < triCount; ++i) {
    //     flip[i] = (out[i] != 0u);
    // }
    
    return flip;
}

}

#endif // USE_BGFX