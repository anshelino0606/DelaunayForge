#ifndef FEM_PING_PONG_STORAGE_BUFFERS_H
#define FEM_PING_PONG_STORAGE_BUFFERS_H

#include "storage_buffer.h"
#include <array>

namespace fem {

template<typename ValueType>
class PingPongStorageBuffers {
public:
    void init(size_t object_count) {
        buffers_[0].init(object_count);
        buffers_[1].init(object_count);
    }

    void destroy() {
        buffers_[0].destroy();
        buffers_[1].destroy();
    }

    bool is_valid() const {
        return buffers_[0].is_valid() && buffers_[1].is_valid();
    }

    void update_all(const std::vector<ValueType>& data) {
        update_current(data);
        update_previous(data);
    }

    void update_current(const std::vector<ValueType>& data) {
        buffers_[0].update(data);
    }

    void update_previous(const std::vector<ValueType>& data) {
        buffers_[1].update(data);
    }

    void update_previous_from_current(size_t obj_count) {
        buffers_[1].update(buffers_[0], obj_count);
    }

    void bind_all(uint8_t curr_bind_point, uint8_t prev_bind_point) const {
        bind_current(curr_bind_point);
        bind_previous(prev_bind_point);
    }

    void bind_current(uint8_t bind_point) const {
        buffers_[0].bind(bind_point, bgfx::Access::ReadWrite);
    }

    void bind_previous(uint8_t bind_point) const {
        buffers_[1].bind(bind_point, bgfx::Access::Read);
    }

    void read_current(std::vector<ValueType>& out_data, size_t object_count) const {
        buffers_[0].read(out_data, object_count);
    }

    void read_previous(std::vector<ValueType>& out_data, size_t object_count) const {
        buffers_[1].read(out_data, object_count);
    }

    uint32_t read_current_no_sync(std::vector<ValueType>& out_data, size_t object_count) const {
        buffers_[0].read_no_sync(out_data, object_count);
    }

    uint32_t read_previous_no_sync(std::vector<ValueType>& out_data, size_t object_count) const {
        buffers_[1].read_no_sync(out_data, object_count);
    }

    void swap() {
        std::swap(buffers_[0], buffers_[1]);
    }

private:
    std::array<StorageBuffer<ValueType>, 2> buffers_;
};

}

#endif // FEM_PING_PONG_STORAGE_BUFFERS_H