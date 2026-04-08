#ifndef FEM_STORAGE_BUFFER_H
#define FEM_STORAGE_BUFFER_H

#if defined(USE_BGFX)

#include "buffer.h"
#include "rhi_context.h"
#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>

namespace fem {

namespace details {

template<typename T>
struct BaseTypeTraits {
    static constexpr size_t element_count = 1;
    static constexpr bool is_integer = std::is_integral_v<T>;
    static constexpr bool is_signed = std::is_signed_v<T>;
    static constexpr bool is_floating = std::is_floating_point_v<T>;
};

// T is the component type (e.g., float)
// L is the dimension (e.g., 4 for vec4)
template<glm::length_t L, typename T, glm::qualifier Q>
struct BaseTypeTraits<glm::vec<L, T, Q>> {
    // For a vector, the element count is its dimension L
    static constexpr size_t element_count = L;
    static constexpr bool is_integer = std::is_integral_v<T>;
    static constexpr bool is_signed = std::is_signed_v<T>;
    static constexpr bool is_floating = std::is_floating_point_v<T>;
};

// C is the number of columns, R is the number of rows
template<glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
struct BaseTypeTraits<glm::mat<C, R, T, Q>> {
    // For a matrix, the element count is Columns * Rows
    static constexpr size_t element_count = C * R;
    static constexpr bool is_integer = std::is_integral_v<T>;
    static constexpr bool is_signed = std::is_signed_v<T>;
    static constexpr bool is_floating = std::is_floating_point_v<T>;
};

template<typename T>
struct StorageBufferTraits {
    using BufferType = DynamicVertexBuffer;
    using TypeTraits = BaseTypeTraits<T>;

    static constexpr size_t s_element_per_object_count = TypeTraits::element_count;

    static BufferType create_gpu_buffer(size_t object_count) {
        static bgfx::VertexLayout layout = get_layout();

        DynamicVertexBufferCreateInfo dvb_info;
        dvb_info.vertex_count = object_count;
        dvb_info.vertex_layout = &layout;
        dvb_info.flags = FEM_BUFFER_FLAG_STORAGE_READ_WRITE;

        BufferType buffer;
        buffer.init(dvb_info);

        return buffer;
    }

    static BufferType create_staging_buffer(size_t object_count) {
        static bgfx::VertexLayout layout = get_layout();

        DynamicVertexBufferCreateInfo dvb_info;
        dvb_info.vertex_count = object_count;
        dvb_info.vertex_layout = &layout;
        dvb_info.flags = FEM_BUFFER_FLAG_RESIZABLE | FEM_BUFFER_FLAG_STAGING;

        BufferType buffer;
        buffer.init(dvb_info);

        return buffer;
    }

    static constexpr bool use_layout() {
        return true;
    }

    static bgfx::VertexLayout get_layout() {
        bgfx::VertexLayout layout;
        layout.begin();
        layout.add(bgfx::Attrib::TexCoord0, s_element_per_object_count, bgfx::AttribType::Float);
        layout.end();
        return layout;
    }

    static size_t get_element_count(size_t object_count) {
        return s_element_per_object_count * object_count;
    }

    static void copy_buffer(size_t object_count) {
        if constexpr (TypeTraits::is_floating) {
            RHIContext::get().copy_buffer_float(get_element_count(object_count));
        }
        if constexpr (TypeTraits::is_signed && TypeTraits::is_integer) {
            RHIContext::get().copy_buffer_int(get_element_count(object_count));
        }
        if constexpr (!TypeTraits::is_signed && TypeTraits::is_integer) {
            RHIContext::get().copy_buffer_uint(get_element_count(object_count));
        }
    }

    static uint32_t read(std::vector<T>& out_data, size_t object_count) {
        size_t element_count = get_element_count(object_count);
        glm::uvec2 texture_size = calc_texture_size(element_count);

        size_t new_object_count = std::ceil(((float)texture_size.x * (float)texture_size.y) / TypeTraits::element_count);
        out_data.resize(new_object_count);

        if constexpr (TypeTraits::is_floating) {
            return RHIContext::get().read_buffer_float(out_data.data(), element_count);
        }
        if constexpr (TypeTraits::is_signed && TypeTraits::is_integer) {
            return RHIContext::get().read_buffer_int(out_data.data(), element_count);
        }
        if constexpr (!TypeTraits::is_signed && TypeTraits::is_integer) {
            return RHIContext::get().read_buffer_uint(out_data.data(), element_count);
        }
    }
};

inline DynamicIndexBuffer create_index_gpu_buffer(size_t object_count, BufferIndexType index_type) {
    DynamicIndexBufferCreateInfo dib_info;

    dib_info.index_count = object_count;
    dib_info.index_type = index_type;
    dib_info.flags = FEM_BUFFER_FLAG_STORAGE_READ_WRITE;

    DynamicIndexBuffer buffer;
    buffer.init(dib_info);

    return buffer;
}

inline DynamicIndexBuffer create_index_staging_buffer(size_t object_count, BufferIndexType index_type) {
    DynamicIndexBufferCreateInfo dib_info;

    dib_info.index_count = object_count;
    dib_info.index_type = index_type;
    dib_info.flags = FEM_BUFFER_FLAG_STAGING | FEM_BUFFER_FLAG_RESIZABLE;

    DynamicIndexBuffer buffer;
    buffer.init(dib_info);

    return buffer;
}

}

template<typename ValueType>
class StorageBuffer { 
public:
    using Traits = details::StorageBufferTraits<ValueType>;
    using BufferType = Traits::BufferType;

    void init(size_t object_count) {
        gpu_buffer_ = Traits::create_gpu_buffer(object_count);
    }

    void destroy() {
        gpu_buffer_.destroy();
    }

    void bind(uint8_t bind_point, bgfx::Access::Enum access) const {
        gpu_buffer_.bind(bind_point, access);
    }

    void grow(size_t new_object_count) {
        if (!is_valid()) {
            init(new_object_count);
            return;
        }

        bgfx::VertexLayout layout = Traits::get_layout();

        if (gpu_buffer_.current_size() < layout.getSize(new_object_count)) {
            BufferType new_buffer = Traits::create_gpu_buffer(new_object_count);

            gpu_buffer_.bind(0, bgfx::Access::Read);
            new_buffer.bind(1, bgfx::Access::Write);

            Traits::copy_buffer(new_object_count);

            gpu_buffer_.destroy();
            gpu_buffer_ = new_buffer;
        }
    }

    void update(const std::vector<ValueType>& data) {
        size_t obj_count = data.size();

        if (!is_valid()) {
            init(obj_count);
        }

        bgfx::VertexLayout layout = Traits::get_layout();

        size_t data_byte_size = layout.getSize(obj_count);
        if (data_byte_size > gpu_buffer_.current_size()) {
            init(obj_count);
        }

        BufferType staging_buffer = Traits::create_staging_buffer(obj_count);

        staging_buffer.update(data.data(), data_byte_size);
        staging_buffer.bind(0, bgfx::Access::Read);
        gpu_buffer_.bind(1, bgfx::Access::Write);

        Traits::copy_buffer(obj_count);

        staging_buffer.destroy();
    }

    void update(const StorageBuffer<ValueType>& src, size_t obj_count) {
        src.bind(0, bgfx::Access::Read);
        gpu_buffer_.bind(1, bgfx::Access::Write);

        Traits::copy_buffer(obj_count);
    }

    void read(std::vector<ValueType>& out_data, size_t object_count) const {
        uint32_t frame_number = read_no_sync(out_data, object_count);

        while (true) {
            if (frame_number == bgfx::frame()) {
                break;
            }
        }
    }

    uint32_t read_no_sync(std::vector<ValueType>& out_data, size_t object_count) const {
        bind(0, bgfx::Access::Read);
        return Traits::read(out_data, object_count);
    }

    bool is_valid() const { 
        return gpu_buffer_.is_valid();
    }

    const BufferType& gpu_buffer() const { return gpu_buffer_; }

private:
    BufferType gpu_buffer_;

};

}

#endif // USE_BGFX

#endif // FEM_STORAGE_BUFFER_H