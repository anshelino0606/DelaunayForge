#include "shader_compilation_context.h"
#include "shader_compiler.h"
#include "log_categories.h"

namespace fem {

ShaderCompilationContext::ShaderCompilationContext(const BaseShaderProgramCreateInfo& create_info) {
    module_ = ShaderCompiler::load_module(create_info.relative_path);
}

void ShaderCompilationContext::add_entry_point(const std::string& entry_point_name) {
    if (EntryPoint entry_point = ShaderCompiler::get_entry_point(module_, entry_point_name.c_str())) {
        entry_points_.push_back(entry_point);
    }
}

void ShaderCompilationContext::compile() {
    link_program();

    for (uint32_t i = 0; i != entry_points_.size(); ++i) {
        compiled_shaders_.push_back(ShaderCompiler::compile_shader(linked_program_, i));
    }
}

ShaderCompilationResult ShaderCompilationContext::get_compiled_shader(uint32_t entry_point_idx) const {
    if (entry_point_idx >= compiled_shaders_.size()) {
        LOGT_ERROR(LogRenderer, "No entry point with idx [%d] for shader [%s]", entry_point_idx, module_->getFilePath());
        return {};
    }

    return {
        .binary = compiled_shaders_[entry_point_idx]->getBufferPointer(),
        .size = compiled_shaders_[entry_point_idx]->getBufferSize()
    };
}

void ShaderCompilationContext::link_program() {
    if (linked_program_) return;
    std::vector<slang::IComponentType*> components(1 + entry_points_.size());
    components[0] = module_;

    for (size_t i = 1; i != components.size(); ++i) {
        components[i] = entry_points_[i - 1];
    }

    ComposedProgram composed_program = ShaderCompiler::compose_program(components.data(), components.size());
    linked_program_ = ShaderCompiler::link_program(composed_program);
}

}