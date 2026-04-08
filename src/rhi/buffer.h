#ifndef FEM_BUFFER_H
#define FEM_BUFFER_H

#if defined(USE_BGFX)

#include "gpu_resource.h"
#include <vector>
#include "log_categories.h"

namespace fem {

enum BufferFlag {
    FEM_BUFFER_FLAG_NONE = 0,
    FEM_BUFFER_FLAG_RESIZABLE = 1 << 1,
    FEM_BUFFER_FLAG_STAGING = 1 << 2,
    FEM_BUFFER_FLAG_STORAGE_READ_WRITE = 1 << 3
};

using BufferFlagBits = uint32_t;

enum class BufferIndexType {
    UINT16,
    UINT32
};

namespace details { 

struct BufferCreateInfo {
    BufferFlagBits flags = FEM_BUFFER_FLAG_NONE;
    const bgfx::Memory* init_memory = nullptr;
};

}

struct StaticVertexBufferCreateInfo : details::BufferCreateInfo {
    const bgfx::VertexLayout* vertex_layout = nullptr;
};

struct DynamicVertexBufferCreateInfo : StaticVertexBufferCreateInfo {
    size_t vertex_count = 0;
};

struct StaticIndexBufferCreateInfo : details::BufferCreateInfo {
    BufferIndexType index_type = BufferIndexType::UINT16;
};

struct DynamicIndexBufferCreateInfo : StaticIndexBufferCreateInfo {
    size_t index_count = 0;
};

namespace details {

inline uint16_t get_bgfx_flags(const BufferCreateInfo& info) {
    uint16_t bgfx_flags = BGFX_BUFFER_NONE;
    if (info.flags & FEM_BUFFER_FLAG_RESIZABLE) {
        bgfx_flags |= BGFX_BUFFER_ALLOW_RESIZE;
    }
    if (info.flags & FEM_BUFFER_FLAG_STAGING) {
        bgfx_flags |= BGFX_BUFFER_COMPUTE_READ;
    }
    if (info.flags & FEM_BUFFER_FLAG_STORAGE_READ_WRITE) {
        bgfx_flags |= BGFX_BUFFER_COMPUTE_READ_WRITE;
    }
    return bgfx_flags;
}

inline uint16_t get_bgfx_flags(const StaticIndexBufferCreateInfo& info) {
    uint16_t bgfx_flags = get_bgfx_flags(static_cast<const BufferCreateInfo&>(info));

    if (info.index_type == BufferIndexType::UINT32) {
        bgfx_flags |= BGFX_BUFFER_INDEX32;
    }
    
    return bgfx_flags;
}

inline bool is_buffer_memory_valid(const BufferCreateInfo& info) {
    if (!info.init_memory) {
        LOGT_ERROR(LogRHI, "Static buffer must have valid memory!");
        return false;
    }

    return true;
}

inline bool is_vertex_layout_valid(const StaticVertexBufferCreateInfo& info) {
    if (!info.vertex_layout) {
        LOGT_ERROR(LogRHI, "Vertex buffer must have valid vertex layout!");
        return false;
    }

    return true;
}

inline size_t get_index_size(const DynamicIndexBufferCreateInfo& info) {
    return info.index_type == BufferIndexType::UINT16 ? sizeof(uint16_t) : sizeof(uint32_t);
}

template <typename T>
struct BgfxBufferTraits;

template <>
struct BgfxBufferTraits<bgfx::VertexBufferHandle>
{
    using HandleType = bgfx::VertexBufferHandle;

    static constexpr const char* s_buffer_name = "StaticVertexBuffer";

    static HandleType create(const StaticVertexBufferCreateInfo& info) {
        if (!is_buffer_memory_valid(info)) {
            return BGFX_INVALID_HANDLE;
        }

        if (!is_vertex_layout_valid(info)) {
            return BGFX_INVALID_HANDLE;
        }

        return bgfx::createVertexBuffer(
            info.init_memory,
            *info.vertex_layout,
            get_bgfx_flags(info)
        );
    }

    static size_t get_size(const StaticVertexBufferCreateInfo& info) {
        if (is_buffer_memory_valid(info)) {
            return 0;
        }

        return info.init_memory->size;
    }

    static void activate(HandleType handle, uint8_t vertex_stream) {
        bgfx::setVertexBuffer(vertex_stream, handle);
    }
};

template <>
struct BgfxBufferTraits<bgfx::DynamicVertexBufferHandle> {
    using HandleType = bgfx::DynamicVertexBufferHandle;

    static constexpr const char* s_buffer_name = "DynamicVertexBuffer";

    static HandleType create(const DynamicVertexBufferCreateInfo& info) {
        if (!is_vertex_layout_valid(info)) {
            return BGFX_INVALID_HANDLE;
        }

        if (info.init_memory) {
            return bgfx::createDynamicVertexBuffer(
                info.init_memory,
                *info.vertex_layout,
                get_bgfx_flags(info)
            );
        }
        
        return bgfx::createDynamicVertexBuffer(
            info.vertex_count,
            *info.vertex_layout,
            get_bgfx_flags(info)
        );
    }

    static size_t get_size(const DynamicVertexBufferCreateInfo& info) {
        if (is_vertex_layout_valid(info)) {
            return 0;
        }

        return info.vertex_layout->getSize(info.vertex_count);
    }

    static void activate(HandleType handle, uint8_t vertex_stream) {
        bgfx::setVertexBuffer(vertex_stream, handle);
    }
};

template<>
struct BgfxBufferTraits<bgfx::IndexBufferHandle> {
    using HandleType = bgfx::IndexBufferHandle;

    static constexpr const char* s_buffer_name = "StaticIndexBuffer";

    static HandleType create(const StaticIndexBufferCreateInfo& info) {
        if (!is_buffer_memory_valid(info)) {
            return BGFX_INVALID_HANDLE;
        }

        return bgfx::createIndexBuffer(
            info.init_memory,
            get_bgfx_flags(info)
        );
    }

    static size_t get_size(const StaticIndexBufferCreateInfo& info) {
        if (is_buffer_memory_valid(info)) {
            return 0;
        }

        return info.init_memory->size;
    }

    static void activate(HandleType handle, uint8_t vertex_stream) {
        bgfx::setIndexBuffer(handle);
    }
};

template<>
struct BgfxBufferTraits<bgfx::DynamicIndexBufferHandle> {
    using HandleType = bgfx::DynamicIndexBufferHandle;

    static constexpr const char* s_buffer_name = "DynamicIndexBuffer";

    static HandleType create(const DynamicIndexBufferCreateInfo& info) {
        if (info.init_memory) {
            return bgfx::createDynamicIndexBuffer(
                info.init_memory,
                get_bgfx_flags(info)
            );
        }

        return bgfx::createDynamicIndexBuffer(
            info.index_count,
            get_bgfx_flags(info)
        );
    }

    static size_t get_size(const DynamicIndexBufferCreateInfo& info) {
        size_t index_size = get_index_size(info);
        return index_size * info.index_count;
    }

    static void activate(HandleType handle, uint8_t vertex_stream) {
        bgfx::setIndexBuffer(handle);
    }
};

template<typename HandleType, typename CreateInfoType>
class TStaticBuffer : public GPUResource<HandleType> {
public:
    using Traits = BgfxBufferTraits<HandleType>;

    void init(const CreateInfoType& info) {
        this->destroy();

        this->handle_ = Traits::create(info);
        current_size_ = Traits::get_size(info);
        flags_ = info.flags;

        if (!this->is_valid()) {
            log_invalid("init");
        }
    }

    void bind(uint8_t bind_point, bgfx::Access::Enum access) const {
        if (!this->is_valid()) {
            log_invalid("bind");
            return;
        }

        bgfx::setBuffer(bind_point, this->handle_, access);
    }

    void activate(uint8_t vertex_stream = 0) const {
        if (!this->is_valid()) {
            log_invalid("activate");
            return;
        }

        Traits::activate(this->handle_, vertex_stream);
    }

    size_t current_size() const { return current_size_; }

protected:
    size_t current_size_ = 0;
    BufferFlagBits flags_ = 0;

    void log_invalid(const char* func_name) const {
        log_msg(func_name, "Buffer is invalid!");
    }

    void log_msg(const char* func_name, const char* msg) const {
        LOGT_ERROR(LogRHI, "%s::%s(): %s\n", Traits::s_buffer_name, func_name, msg);
    }

    bool is_resizable() const {
        return flags_ & FEM_BUFFER_FLAG_RESIZABLE;
    }
};

template<typename HandleType, typename CreateInfoType>
class TDynamicBuffer : public TStaticBuffer<HandleType, CreateInfoType> {
public:
    void update(const void* data, size_t data_size) {
        if (this->is_resizable() || data_size <= this->current_size_) {
            bgfx::update(this->handle_, 0, bgfx::copy(data, data_size));
        } else {
            this->log_msg("update", "Does not support non resizable dynamic buffers!");
        }
    }

    template<typename T>
    void update(const std::vector<T>& data)
    {
        update(data.data(), data.size() * sizeof(T));
    }
};

}

using StaticVertexBuffer = details::TStaticBuffer<bgfx::VertexBufferHandle, StaticVertexBufferCreateInfo>;
using DynamicVertexBuffer = details::TDynamicBuffer<bgfx::DynamicVertexBufferHandle, DynamicVertexBufferCreateInfo>;
using StaticIndexBuffer = details::TStaticBuffer<bgfx::IndexBufferHandle, StaticIndexBufferCreateInfo>;
using DynamicIndexBuffer = details::TDynamicBuffer<bgfx::DynamicIndexBufferHandle, DynamicIndexBufferCreateInfo>;

}

#endif // USE_BGFX
#endif // FEM_BUFFER_H