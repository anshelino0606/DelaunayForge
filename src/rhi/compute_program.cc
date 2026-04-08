#include "compute_program.h"
#include "shader_module.h"
#include "log_categories.h"

#if defined(USE_BGFX)

namespace fem {

void ComputeProgram::init(const std::string& compute_shader_path) {
    ShaderModule shader;
    shader.init(compute_shader_path);

    if (shader.is_valid()) {
        handle_ = bgfx::createProgram(shader.handle(), true);
    } else {
        LOGT_ERROR(LogRHI, "ComputeProgram::init(): Failed to create compute program from shader: %s", compute_shader_path.c_str());
    }
}

void ComputeProgram::dispatch(uint32_t count_x, uint32_t count_y, uint32_t count_z, bgfx::ViewId view_id) const {
    if (!is_valid()) {
        LOGT_ERROR(LogRHI, "ComputeProgram::dispatch(): Compute program is invalid!");
        return;
    }

    bgfx::dispatch(view_id, handle_, count_x, count_y, count_z);
}

void ComputeProgram::dispatch(bgfx::IndirectBufferHandle indirect_buffer, bgfx::ViewId view_id) const {
    if (!is_valid()) {
        LOGT_ERROR(LogRHI, "ComputeProgram::dispatch(): Compute program is invalid!");
        return;
    }

    bgfx::dispatch(view_id, handle_, indirect_buffer, 1);
}

}

#endif // USE_BGFX