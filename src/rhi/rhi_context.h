#ifndef FEM_RHI_CONTEXT_H
#define FEM_RHI_CONTEXT_H

#if defined(USE_BGFX)

#include "compute_program.h"
#include "texture.h"
#include <glm/vec2.hpp>
#include <string>
#include <cmath>

namespace fem {

struct CopyBufferContext {
    ComputeProgram cs_copy_buffer;
    bgfx::UniformHandle params;

    void init(const char* shader_path, bgfx::UniformHandle uniform_handle);
    void dispatch(size_t count) const;
};

struct ReadBufferContext {
    Texture storage_texture;
    bgfx::TextureFormat::Enum texture_format = bgfx::TextureFormat::Enum::Unknown;
    ComputeProgram cs_read_buffer;
    bgfx::UniformHandle params;
    
    void init(const char* shader_path, bgfx::TextureFormat::Enum in_texture_format, bgfx::UniformHandle uniform_handle);
    void destroy();

    uint32_t read(void* out_data, size_t count) const;
};

inline glm::uvec2 calc_texture_size(size_t element_count) {
    uint32_t texture_width = uint32_t(std::ceil(std::sqrt(element_count)));
    uint32_t texture_height = uint32_t(((float)element_count + texture_width - 1) / texture_width);

    return { texture_width, texture_height };
}

class RHIContext { 
public:
    static RHIContext& get() {
        static RHIContext instance;
        return instance;
    }
    
    void init();
    void destroy();

    std::string shader_dir() const {
        return SHADER_DIR;
    }

    void copy_buffer_float(size_t count) {
        copy_buffer_float_ctx_.dispatch(count);
    }

    void copy_buffer_uint(size_t count) const {
        copy_buffer_uint_ctx_.dispatch(count);
    }

    void copy_buffer_int(size_t count) const {
        copy_buffer_int_ctx_.dispatch(count);
    }

    uint32_t read_buffer_float(void* out_data, size_t count) {
        return read_buffer_float_ctx_.read(out_data, count);
    }

    uint32_t read_buffer_uint(void* out_data, size_t count) {
        return read_buffer_uint_ctx_.read(out_data, count);
    }

    // TODO: Implement read for signed integers
    uint32_t read_buffer_int(void* out_data, size_t count) {
        return read_buffer_int_ctx_.read(out_data, count);
    }
    
private:
    CopyBufferContext copy_buffer_float_ctx_;
    CopyBufferContext copy_buffer_uint_ctx_;
    CopyBufferContext copy_buffer_int_ctx_;
    bgfx::UniformHandle copy_buffer_params_;

    ReadBufferContext read_buffer_float_ctx_;
    ReadBufferContext read_buffer_int_ctx_;
    ReadBufferContext read_buffer_uint_ctx_;
    bgfx::UniformHandle read_buffer_params_;

    RHIContext() = default;

    RHIContext(const RHIContext&) = delete;
    RHIContext& operator=(const RHIContext&) = delete;
    RHIContext(RHIContext&&) = delete;
    RHIContext& operator=(RHIContext&&) = delete;
};

}

#endif // USE_BGFX

#endif // FEM_RHI_CONTEXT_H