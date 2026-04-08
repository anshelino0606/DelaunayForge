#include "rhi_context.h"
#include <cmath>

#if defined(USE_BGFX)

namespace fem {

constexpr const char* CS_COPY_BUFFER_FLOAT_PATH = "cs_copy_buffer_float.bin";
constexpr const char* CS_COPY_BUFFER_UINT_PATH = "cs_copy_buffer_uint.bin";
constexpr const char* CS_COPY_BUFFER_INT_PATH = "cs_copy_buffer_int.bin";
constexpr const char* CS_COPY_BUFFER_UNIFORM_NAME = "u_copyBuffersParams";

constexpr const char* CS_READ_BUFFER_FLOAT_PATH = "cs_read_buffer_float.bin";
constexpr const char* CS_READ_BUFFER_UINT_PATH = "cs_read_buffer_uint.bin";
constexpr const char* CS_READ_BUFFER_INT_PATH = "cs_read_buffer_int.bin";
constexpr const char* READ_BUFFER_UNIFORM_NAME = "u_readBufferParams";

constexpr bgfx::ViewId COPY_TO_TEXTURE_PASS = 1;
constexpr bgfx::ViewId BLIT_PASS = 2;

constexpr uint16_t READ_BUFFER_TEXTURE_WIDTH = 2048;
constexpr uint16_t READ_BUFFER_TEXTURE_HEIGHT = 2048;

constexpr uint32_t READ_BUFFER_THREAD_COUNT = 8;    // Move to shader_interop file?

void RHIContext::init() {
    copy_buffer_params_ = bgfx::createUniform(
        CS_COPY_BUFFER_UNIFORM_NAME, 
        bgfx::UniformType::Vec4
    );

    read_buffer_params_ = bgfx::createUniform(
        READ_BUFFER_UNIFORM_NAME, 
        bgfx::UniformType::Vec4
    );

    copy_buffer_float_ctx_.init(CS_COPY_BUFFER_FLOAT_PATH, copy_buffer_params_);
    copy_buffer_uint_ctx_.init(CS_COPY_BUFFER_UINT_PATH, copy_buffer_params_);
    copy_buffer_int_ctx_.init(CS_COPY_BUFFER_INT_PATH, copy_buffer_params_);

    read_buffer_float_ctx_.init(
        CS_READ_BUFFER_FLOAT_PATH, 
        bgfx::TextureFormat::R32F, 
        read_buffer_params_
    );

    read_buffer_uint_ctx_.init(
        CS_READ_BUFFER_UINT_PATH, 
        bgfx::TextureFormat::R32U, 
        read_buffer_params_
    );

    // read_buffer_int_ctx_.init(
    //     CS_READ_BUFFER_INT_PATH, 
    //     bgfx::TextureFormat::R32I, 
    //     read_buffer_params_
    // );
}

void RHIContext::destroy() {
    copy_buffer_float_ctx_.cs_copy_buffer.destroy();
    copy_buffer_uint_ctx_.cs_copy_buffer.destroy();
    copy_buffer_int_ctx_.cs_copy_buffer.destroy();
    bgfx::destroy(copy_buffer_params_);

    read_buffer_float_ctx_.destroy();
    read_buffer_int_ctx_.destroy();
    read_buffer_uint_ctx_.destroy();
    bgfx::destroy(read_buffer_params_);
}

glm::uvec2 get_thread_groups_count(glm::uvec2 texture_size) {
    return {
        (texture_size.x + READ_BUFFER_THREAD_COUNT - 1) / READ_BUFFER_THREAD_COUNT,
        (texture_size.y + READ_BUFFER_THREAD_COUNT - 1) / READ_BUFFER_THREAD_COUNT,
    };
}

void CopyBufferContext::init(const char* shader_path, bgfx::UniformHandle uniform_handle) {
    cs_copy_buffer.init(shader_path);
    params = uniform_handle;
}

void CopyBufferContext::dispatch(size_t count) const {
    float params_data[4] = { (float)count, 0.0f, 0.0f, 0.0f };
    bgfx::setUniform(params, &params_data);

    if (uint32_t groupXCount = (count + 63u) / 64){
        cs_copy_buffer.dispatch(groupXCount);
    }
}

void ReadBufferContext::init(const char* shader_path, bgfx::TextureFormat::Enum in_texture_format, bgfx::UniformHandle uniform_handle) {
    cs_read_buffer.init(shader_path);
    params = uniform_handle;
    texture_format = in_texture_format;

    TextureCreateInfo texture_info;
    texture_info.width = READ_BUFFER_TEXTURE_WIDTH;
    texture_info.height = READ_BUFFER_TEXTURE_HEIGHT;
    texture_info.format = texture_format;
    texture_info.flags = FEM_TEXTURE_FLAG_STORAGE;

    storage_texture.init(texture_info);
}

void ReadBufferContext::destroy() {
    storage_texture.destroy();
    cs_read_buffer.destroy();
}

uint32_t ReadBufferContext::read(void* out_data, size_t count) const {
    glm::uvec2 texture_size = calc_texture_size(count);

    float texture_width = texture_size.x;
    float texture_height = texture_size.y;

    glm::uvec2 thread_groups_count = get_thread_groups_count(texture_size);

    if (thread_groups_count.x && thread_groups_count.y) {
        float view_rect[4] = { texture_width, texture_height, (float)count, 0.0f };
        bgfx::setUniform(params, view_rect);

        storage_texture.bind(2, bgfx::Access::Write, texture_format);
        cs_read_buffer.dispatch(thread_groups_count.x, thread_groups_count.y, 1, COPY_TO_TEXTURE_PASS);
    }

    TextureCreateInfo read_back_texture_info;
    read_back_texture_info.width = texture_width;
    read_back_texture_info.height = texture_height;
    read_back_texture_info.format = texture_format;
    read_back_texture_info.flags = FEM_TEXTURE_FLAG_READ_BACK;

    Texture read_buffer_cpu_texture;
    read_buffer_cpu_texture.init(read_back_texture_info);

    bgfx::blit(
        BLIT_PASS,
        read_buffer_cpu_texture.handle(),
        0,
        0,
        storage_texture.handle(),
        0,
        0,
        texture_width,
        texture_height
    );

    uint32_t frame_number = read_buffer_cpu_texture.read(out_data, count * sizeof(uint32_t));

    read_buffer_cpu_texture.destroy();

    return frame_number;
}

}

#endif // USE_BGFX