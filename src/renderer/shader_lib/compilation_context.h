#pragma once

#include "types.h"
#include "renderer/rhi/shader_types.h"
#include <slang-com-ptr.h>
#include <slang.h>

namespace fem::shaderlib {

struct CompilationResult {
    const void* binary = nullptr;
    size_t size = 0;
};

class CompilationContext {
public:
    CompilationContext(const BaseShaderProgramCreateInfo& create_info);

    void add_entry_point(const std::string& entry_point_name);
    void compile();

    CompilationResult get_compiled_shader(uint32_t entry_point_idx) const;

private:
    LoadedModuleInfo loaded_module_info_;
    ComPtr<LinkedProgram> linked_program_;
    std::vector<ComPtr<EntryPoint>> entry_points_;
    std::vector<ComPtr<CompiledBlob>> compiled_shaders_;

    void link_program();
};

}