#include "graphics_program.h"
#include "shader_module.h"
#include "log_categories.h"

#if defined(USE_BGFX)

namespace fem {

void GraphicsProgram::init(
    const std::string& vertex_shader_path,
    const std::string& fragment_shader_path
) {
    ShaderModule vertex_shader, fragment_shader;
    vertex_shader.init(vertex_shader_path);
    fragment_shader.init(fragment_shader_path);

    if (vertex_shader.is_valid() && fragment_shader.is_valid()) {
        handle_ = bgfx::createProgram(
            vertex_shader.handle(),
            fragment_shader.handle(),
            true
        );
    }

    if (!vertex_shader.is_valid()) {
        LOGT_ERROR(LogRHI, "GraphicsProgram::init(): Failed to create graphics program from vertex shader: %s", vertex_shader_path.c_str());
    }

    if (!fragment_shader.is_valid()) {
        LOGT_ERROR(LogRHI, "GraphicsProgram::init(): Failed to create graphics program from fragment shader: %s", vertex_shader_path.c_str());
    }
}

void GraphicsProgram::submit(bgfx::ViewId view_id, uint32_t depth, uint8_t flags) const {
    if (!is_valid()) {
        LOGT_ERROR(LogRHI, "GraphicsProgram::dispatch(): Graphics program is invalid!");
        return;
    }

    bgfx::submit(view_id, handle_, depth, flags);
}

}

#endif // USE_BGFX