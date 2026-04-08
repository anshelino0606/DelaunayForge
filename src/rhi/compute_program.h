#ifndef FEM_COMPUTE_PROGRAM_H
#define FEM_COMPUTE_PROGRAM_H

#if defined(USE_BGFX)

#include "gpu_resource.h"
#include <string>

namespace fem {

class ComputeProgram : public details::GPUResource<bgfx::ProgramHandle> {
public:
    void init(const std::string& compute_shader_path);

    void dispatch(uint32_t count_x = 1, uint32_t count_y = 1, uint32_t count_z = 1, bgfx::ViewId view_id = 0) const;
    void dispatch(bgfx::IndirectBufferHandle indirect_buffer, bgfx::ViewId view_id = 0) const;
};

}

#endif

#endif // FEM_COMPUTE_PROGRAM_H