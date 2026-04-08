#ifndef FEM_GRAPHICS_PROGRAM_H
#define FEM_GRAPHICS_PROGRAM_H

#if defined(USE_BGFX)

#include "gpu_resource.h"
#include <string>

namespace fem {
    
class GraphicsProgram : public details::GPUResource<bgfx::ProgramHandle> {
public:
    void init(
        const std::string& vertex_shader_path,
        const std::string& fragment_shader_path
    );

    void submit(bgfx::ViewId view_id = 0, uint32_t depth = 0, uint8_t flags = BGFX_DISCARD_ALL) const;
};

}

#endif // USE_BGFX

#endif // FEM_GRAPHICS_PROGRAM_H