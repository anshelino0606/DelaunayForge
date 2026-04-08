#include "shader_module.h"
#include "rhi_context.h"
#include "log_categories.h"
#include <iostream>

#if defined(USE_BGFX)

namespace fem {

void ShaderModule::init(const std::string& relative_shader_path) {
    std::string path = RHIContext::get().shader_dir() + "/" + relative_shader_path;

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        LOGT_ERROR(LogRHI, "Shader::init(): Failed to open shader file: %s", path.c_str());
        return;
    }
    
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    
    const bgfx::Memory* mem = bgfx::alloc((uint32_t)sz + 1);
    std::fread(mem->data, 1, (size_t)sz, f);
    std::fclose(f);
    mem->data[sz] = '\0';
    
    handle_ = bgfx::createShader(mem);

    if (!is_valid()) {
        LOGT_ERROR(LogRHI, "Failed to create shader from %s\n", path.c_str());
    }
}

}


#endif