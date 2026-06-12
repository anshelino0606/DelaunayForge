#include "shader_manager.h"
#include "shader_compiler.h"

namespace fem {

ShaderManager::ShaderManager() {
    ShaderCompiler::init();
}

ShaderManager::~ShaderManager() {
    graphics_shader_programs_.clear();
    ShaderCompiler::shutdown();
}

GraphicsShaderProgram* ShaderManager::graphics_shader_program(const GraphicsShaderProgramCreateInfo& create_info) {
    return get_shader_program(graphics_shader_programs_, create_info);
}

}