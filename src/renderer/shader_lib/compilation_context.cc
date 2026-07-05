#include "compilation_context.h"
#include "compiler.h"
#include "log_categories.h"

namespace fem::shaderlib {

CompilationContext::CompilationContext(const BaseShaderProgramCreateInfo& create_info) {
    loaded_module_info_ = compiler::load_module(create_info.relative_path);
}

void CompilationContext::add_entry_point(const std::string& entry_point_name) {
    if (ComPtr<EntryPoint> entry_point = compiler::get_entry_point(loaded_module_info_.module, entry_point_name.c_str())) {
        entry_points_.push_back(entry_point);
    }
}

void CompilationContext::compile() {
    link_program();

    for (uint32_t i = 0; i != entry_points_.size(); ++i) {
        compiled_shaders_.push_back(compiler::compile_shader(linked_program_, i));
    }
}

CompilationResult CompilationContext::get_compiled_shader(uint32_t entry_point_idx) const {
    if (entry_point_idx >= compiled_shaders_.size()) {
        LOGT_ERROR(LogRenderer, "No entry point with idx [%d] for shader [%s]", entry_point_idx, loaded_module_info_.module->getFilePath());
        return {};
    }

    return {
        .binary = compiled_shaders_[entry_point_idx]->getBufferPointer(),
        .size = compiled_shaders_[entry_point_idx]->getBufferSize()
    };
}

void CompilationContext::link_program() {
    if (linked_program_) return;
    std::vector<ProgramComponent*> components(1 + entry_points_.size());
    components[0] = loaded_module_info_.module;

    for (size_t i = 1; i != components.size(); ++i) {
        components[i] = entry_points_[i - 1];
    }

    ComPtr<ComposedProgram> composed_program = compiler::compose_program(components.data(), components.size());
    linked_program_ = compiler::link_program(composed_program);
}

}