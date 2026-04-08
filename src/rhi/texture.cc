#include "texture.h"
#include "log_categories.h"

#if defined(USE_BGFX)

namespace fem {

void Texture::init(const TextureCreateInfo& create_info) {
    destroy();

    handle_ = bgfx::createTexture2D(
        create_info.width,
        create_info.height,
        create_info.has_mips,
        create_info.layer_count,
        create_info.format,
        get_bgfx_flags(create_info),
        create_info.init_memory
    );
}

void Texture::bind(uint8_t bind_point, bgfx::Access::Enum access, bgfx::TextureFormat::Enum format, uint8_t mipLevel) const {
    if (!is_valid()) {
        // TODO: maybe demote some of those warn logs here to DEBUG
        LOGT_WARN(LogRHI, "Texture::bind(): Texture is invalid!");
        return;
    }

    bgfx::setImage(
        bind_point, 
        handle_,
        mipLevel,
        access,
        format
    );
}

void Texture::bind(uint8_t bind_point, bgfx::UniformHandle sampler) const {
    if (!is_valid()) {
        LOGT_WARN(LogRHI, "Texture::bind(): Texture is invalid!");
        return;
    }

    bgfx::setTexture(
        bind_point, 
        sampler,
        handle_
    );
}

uint32_t Texture::read(void* out_data, uint32_t data_size) const {
    // TODO: Implement size validations. Need to find how to get format stride.
    if (!is_valid()) {
        LOGT_WARN(LogRHI, "Texture::read(): Texture is invalid!");
        return 0;
    }

    return bgfx::readTexture(handle_, out_data);
}

bgfx::FrameBufferHandle Texture::get_frame_buffer() const {
    if (!is_valid()) {
        LOGT_WARN(LogRHI, "Texture::get_frame_buffer(): Texture is invalid!");
        return BGFX_INVALID_HANDLE;
    }

    return bgfx::createFrameBuffer(1, &handle_);
}

void Texture::set_handle_index(uint64_t new_idx) {
    if (!is_valid()) {
        LOGT_WARN(LogRHI, "Texture::set_handle_index(): Texture is invalid!");
        return;
    }

    handle_.idx = new_idx;
}

uint64_t Texture::get_bgfx_flags(const TextureCreateInfo& create_info) const {
    uint64_t bgfx_flags = BGFX_TEXTURE_NONE;

    if (create_info.flags & FEM_TEXTURE_FLAG_RT) {
        bgfx_flags |= BGFX_TEXTURE_RT;
    }
    if (create_info.flags & FEM_TEXTURE_FLAG_READ_BACK) {
        bgfx_flags |= BGFX_TEXTURE_READ_BACK;
        bgfx_flags |= BGFX_TEXTURE_BLIT_DST;
    }
    if (create_info.flags & FEM_TEXTURE_FLAG_SAMPLER_POINT) {
        bgfx_flags |= BGFX_SAMPLER_POINT;
    }
    if (create_info.flags & FEM_TEXTURE_FLAG_STORAGE) {
        bgfx_flags |= BGFX_TEXTURE_COMPUTE_WRITE;
    }
    if (create_info.flags & FEM_TEXTURE_FLAG_SAMPLER_MIN_POINT) {
        bgfx_flags |= BGFX_SAMPLER_MIN_POINT;
    }
    if (create_info.flags & FEM_TEXTURE_FLAG_SAMPLER_MAG_POINT) {
        bgfx_flags |= BGFX_SAMPLER_MAG_POINT;
    }

    return bgfx_flags;
}

}

#endif // USE_BGFX