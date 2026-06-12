#pragma once

#include "shader_types.h"
#include <slang-com-ptr.h>
#include <slang.h>

namespace fem {

struct ShaderCompilationResult {
    const void* binary = nullptr;
    size_t size = 0;
};

class ShaderCompilationContext {
public:
    ShaderCompilationContext(const BaseShaderProgramCreateInfo& create_info);

    void add_entry_point(const std::string& entry_point_name);
    void compile();

    ShaderCompilationResult get_compiled_shader(uint32_t entry_point_idx) const;

private:
    using ProgramModule = Slang::ComPtr<slang::IModule>;
    using EntryPoint = Slang::ComPtr<slang::IEntryPoint>;
    using ComposedProgram = Slang::ComPtr<slang::IComponentType>;
    using LinkedProgram = Slang::ComPtr<slang::IComponentType>;
    using CompiledShader = Slang::ComPtr<slang::IBlob>;

    ProgramModule module_;
    LinkedProgram linked_program_;
    std::vector<EntryPoint> entry_points_;
    std::vector<CompiledShader> compiled_shaders_;

    void link_program();
};

    
}