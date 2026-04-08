#ifndef FEM_RHI_RESOURCE_H
#define FEM_RHI_RESOURCE_H

#if defined(USE_BGFX)

#include <bgfx/bgfx.h>

namespace fem::details {

template<typename HandleType>
class GPUResource {
public:
    void destroy() {
        if (is_valid()) {
            bgfx::destroy(handle_);
            handle_ = BGFX_INVALID_HANDLE;
        }
    }

    bool is_valid() const { return bgfx::isValid(handle_); }
    HandleType handle() const { return handle_; }

protected:
    HandleType handle_ = BGFX_INVALID_HANDLE;
};

}

#endif // USE_BGFX

#endif // FEM_RHI_RESOURCE_H