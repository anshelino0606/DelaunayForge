#include "compilation_context.h"
#include "compiler.h"
#include "utils.h"
#include "vector_blob.h"
#include "log_categories.h"
#include "core/file_system/file_system.h"

namespace fem::shaderlib {

CompilationContext::CompilationContext(const BaseShaderProgramCreateInfo& create_info) {
    relative_path_ = create_info.relative_path;
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
        if (loaded_module_info_.is_cache_valid) {
            load_shader_from_cache(i);
        } else {
            compile_and_cache_shader(i);
        }
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

void CompilationContext::compile_and_cache_shader(uint32_t entry_point_idx) {
    ComPtr<CompiledBlob> bin_blob = compiler::compile_shader(linked_program_, entry_point_idx);

    std::string shader_bin_relative_path = get_shader_bin_relative_path(entry_point_idx);
    std::string shader_bin_absolute_path = get_and_prepare_cached_shader_bin_path(shader_bin_relative_path);
    FileSystem::write(
        shader_bin_absolute_path,
        static_cast<const uint8_t*>(bin_blob->getBufferPointer()),
        bin_blob->getBufferSize()
    );

    compiled_shaders_.push_back(bin_blob);
}

void CompilationContext::load_shader_from_cache(uint32_t entry_point_idx) {
    std::string shader_bin_relative_path = get_shader_bin_relative_path(entry_point_idx);
    std::string shader_bin_absolute_path = get_cached_shader_bin_path(shader_bin_relative_path);

    std::vector<uint8_t> shader_bin_data;
    FileSystem::read(shader_bin_absolute_path, shader_bin_data);

    ComPtr<CompiledBlob> bin_blob;
    bin_blob.attach(new VectorBlob(std::move(shader_bin_data)));

    compiled_shaders_.push_back(bin_blob);
}

std::string CompilationContext::get_shader_bin_relative_path(uint32_t entry_point_idx) {
    ProgramLayout* program_layout = linked_program_->getLayout();
    EntryPointRefl* entry_point_refl = program_layout->getEntryPointByIndex(entry_point_idx);
    return std::format("{}_{}", relative_path_, entry_point_refl->getName());
}

}