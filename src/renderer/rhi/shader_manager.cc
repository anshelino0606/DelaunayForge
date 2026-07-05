#include "shader_manager.h"
#include "renderer/shader_lib/compiler.h"

namespace fem {

ShaderManager::ShaderManager() {
    shaderlib::compiler::init();
}

ShaderManager::~ShaderManager() {
    graphics_shader_programs_.clear();
    shaderlib::compiler::shutdown();
}

GraphicsShaderProgram* ShaderManager::graphics_shader_program(const GraphicsShaderProgramCreateInfo& create_info) {
    return get_shader_program(graphics_shader_programs_, create_info);
}

}