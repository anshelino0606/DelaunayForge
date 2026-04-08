#ifndef FEM_TEXTURE_H
#define FEM_TEXTURE_H

#if defined(USE_BGFX)

#include "gpu_resource.h"
#include <vector>

namespace fem {

enum TextureFlag : uint64_t{
    FEM_TEXTURE_FLAG_NONE = 0,
    FEM_TEXTURE_FLAG_RT = 1 << 1,
    FEM_TEXTURE_FLAG_STORAGE = 1 << 2,
    FEM_TEXTURE_FLAG_SAMPLER_POINT = 1 << 3,
    FEM_TEXTURE_FLAG_READ_BACK = 1 << 4,
    FEM_TEXTURE_FLAG_SAMPLER_MIN_POINT = 1 << 5,
    FEM_TEXTURE_FLAG_SAMPLER_MAG_POINT = 1 << 6
};

using TextureFlagBits = uint64_t;

struct TextureCreateInfo {
    uint16_t width = 0;
    uint16_t height = 0;
    bool has_mips = false;
    uint16_t layer_count = 1;
    bgfx::TextureFormat::Enum format = bgfx::TextureFormat::Unknown;
    TextureFlagBits flags = FEM_TEXTURE_FLAG_NONE;
    const bgfx::Memory* init_memory = nullptr;
};

class Texture : public details::GPUResource<bgfx::TextureHandle> {
public:
    void init(const TextureCreateInfo& create_info);

    void bind(uint8_t bind_point, bgfx::Access::Enum access, bgfx::TextureFormat::Enum format, uint8_t mipLevel = 0) const;
    void bind(uint8_t bind_point, bgfx::UniformHandle sampler) const;
    uint32_t read(void* out_data, uint32_t data_size) const;
    
    template<typename T>
    uint32_t read(std::vector<T>& out_vec) const {
        return read(out_vec.data(), out_vec.size() * sizeof(T));
    }

    bgfx::FrameBufferHandle get_frame_buffer() const;

    void set_handle_index(uint64_t new_idx);

protected:
    uint64_t get_bgfx_flags(const TextureCreateInfo& create_info) const;
};
}

#endif // USE_BGFX

#endif // FEM_TEXTURE_H