#ifndef FEM_SHADER_MODULE_H
#define FEM_SHADER_MODULE_H

#if defined(USE_BGFX)

#include "gpu_resource.h"
#include <string>

namespace fem {

class ShaderModule : public details::GPUResource<bgfx::ShaderHandle> {
public:
    void init(const std::string& relative_shader_path);
};

}

#endif // USE_BGFX

#endif // FEM_SHADER_H