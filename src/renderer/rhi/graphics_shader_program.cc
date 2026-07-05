#include "graphics_shader_program.h"
#include "renderer/shader_lib/compilation_context.h"
#include "renderer/device.h"
#include "core/utils.h"

namespace fem {

GraphicsShaderProgram::GraphicsShaderProgram(const GraphicsShaderProgramCreateInfo& create_info) {
    create(create_info);
}

void GraphicsShaderProgram::create(const GraphicsShaderProgramCreateInfo& create_info) {
    const char* vertex_entry_point_name = create_info.vertex_entry_point.c_str();
    const char* fragment_entry_point_name = create_info.fragment_entry_point.c_str();

    shaderlib::CompilationContext compilation_context(create_info);
    compilation_context.add_entry_point(vertex_entry_point_name);
    compilation_context.add_entry_point(fragment_entry_point_name);
    compilation_context.compile();

    shaderlib::CompilationResult vs_compilation_result = compilation_context.get_compiled_shader(0);
    shaderlib::CompilationResult fs_compilation_result = compilation_context.get_compiled_shader(1);

    vertex_shader_.create(Shader::InitInfo{
        .data = vs_compilation_result.binary,
        .data_size = vs_compilation_result.size,
        .debug_name = create_info.relative_path,
        .entry_point = create_info.vertex_entry_point,
        .type = LLGL::ShaderType::Vertex,
        .vertex_attribs = &g_vertex_layouts[create_info.vertex_layout]
    });

    fragment_shader_.create(Shader::InitInfo{
        .data = fs_compilation_result.binary,
        .data_size = fs_compilation_result.size,
        .debug_name = create_info.relative_path,
        .entry_point = create_info.fragment_entry_point,
        .type = LLGL::ShaderType::Fragment,
    });
}

std::size_t GraphicsShaderProgramCreateInfoHasher::operator()(const GraphicsShaderProgramCreateInfo& info) const noexcept {
    size_t seed = BaseShaderProgramCreateInfoHasher()(info);
    Utils::hash_combine(seed, std::hash<std::string>{}(info.vertex_entry_point));
    Utils::hash_combine(seed, std::hash<std::string>{}(info.fragment_entry_point));
    return seed;
}

}