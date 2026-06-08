#include "shader_manager.h"

namespace fem {

constexpr const char* g_pixel_entry_point = "fragment_main";
constexpr const char* g_compute_entry_point = "kernel_main";

ShaderManager::~ShaderManager() {
    shaders_.clear();
}

Shader* ShaderManager::get_pixel_shader(const ShaderCompileInfo&  info) {
    return get_shader(info, g_pixel_entry_point, LLGL::ShaderType::Fragment);
}

Shader* ShaderManager::get_compute_shader(const ShaderCompileInfo&  info) {
    return get_shader(info, g_compute_entry_point, LLGL::ShaderType::Compute);
}

Shader* ShaderManager::get_shader(
    const ShaderCompileInfo& info, 
    std::string_view default_entry_point, 
    LLGL::ShaderType type,
    VertexLayout vertex_layout
) {
    ShaderCompileInfo shader_key = info;
    if (shader_key.entry_point == std::nullopt) {
        shader_key.entry_point = default_entry_point;
    }

    auto it = shaders_.find(shader_key);
    if (it != shaders_.end()) {
        return it->second.get();
    }

    Slang::ComPtr<slang::IBlob> compiled_code = shader_compiler_.compile(shader_key);

    return shaders_.emplace(shader_key, new Shader(Shader::InitInfo{
        .data = compiled_code->getBufferPointer(),
        .data_size = compiled_code->getBufferSize(),
        .debug_name = shader_key.relative_path,
        .entry_point = *shader_key.entry_point,
        .type = type,
        .vertex_attribs = &g_vertex_layouts[vertex_layout]
    })).first->second.get();
}

}