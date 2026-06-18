#include "shader_compiler.h"
#include "utils.h"
#include "shader_compilation_global_session.h"
#include "core/file_system/file_system.h"
#include "log_categories.h"
#include <format>

namespace fem {

void ShaderCompiler::init() {
#if defined(_WIN32)
    s_session_ = ShaderCompilationSessionHandle::create(GraphicsAPI::D3D12);
#elif defined(__APPLE__)
    s_session_ = ShaderCompilationSessionHandle::create(GraphicsAPI::METAL);
#endif
}

void ShaderCompiler::shutdown() {
    s_session_->handle()->release();
    ShaderCompilationGlobalSession::instance()->release();
    slang::shutdown();
}

Slang::ComPtr<slang::IModule> ShaderCompiler::load_module(const std::string& relative_path) {
    ShaderModuleLoadInfo load_info = module_load_info_from_relative_path(relative_path);

    std::string shader_content;
    FileSystem::read(load_info.absolute_path, shader_content);

    Slang::ComPtr<slang::IBlob> diagnostics_blob;

    Slang::ComPtr<slang::IModule> slang_module;
    slang_module = s_session_->handle()->loadModule(
        load_info.name.c_str(), 
        diagnostics_blob.writeRef()
    );
    log_diagnostics(diagnostics_blob);
    if (!slang_module) {
        LOGT_ERROR(LogRenderer, "Failed to load slang module [%s]", load_info.relative_path.c_str());
    }

    return slang_module;
}

Slang::ComPtr<slang::IEntryPoint> ShaderCompiler::get_entry_point(slang::IModule* module, const char* entry_point_name) {
    Slang::ComPtr<slang::IEntryPoint> entry_point;
    module->findEntryPointByName(entry_point_name, entry_point.writeRef());
    if (!entry_point) {
        LOGT_ERROR(LogRenderer, "Failed to find entry point [%s] for shader [%s]!", entry_point_name, module->getFilePath());
    }
    return entry_point;
}

Slang::ComPtr<slang::IComponentType> ShaderCompiler::compose_program(slang::IComponentType* const* component_types, uint32_t component_type_count) {
    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    Slang::ComPtr<slang::IComponentType> composed_program;

    SlangResult result = s_session_->handle()->createCompositeComponentType(
        component_types, 
        component_type_count, 
        composed_program.writeRef(), 
        diagnostics_blob.writeRef()
    );
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);
    return composed_program;
}

Slang::ComPtr<slang::IComponentType> ShaderCompiler::link_program(slang::IComponentType* composed_program) {
    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    Slang::ComPtr<slang::IComponentType> linked_program;

    SlangResult result = composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);
    return linked_program;
}

Slang::ComPtr<slang::IBlob> ShaderCompiler::compile_shader(slang::IComponentType* linked_program, uint32_t entry_point_idx) {
    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    Slang::ComPtr<slang::IBlob> compiled_code;

    SlangResult result = linked_program->getEntryPointCode(entry_point_idx, 0, compiled_code.writeRef(), diagnostics_blob.writeRef());
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);
    return compiled_code;
}

ShaderCompiler::ShaderModuleLoadInfo ShaderCompiler::module_load_info_from_relative_path(const std::string& relative_path) {
    ShaderModuleLoadInfo module_info;
    
    module_info.name = FileSystem::get_file_name(relative_path);
    module_info.relative_path = std::format("shaders/{}.slang", relative_path);
    module_info.absolute_path = FileSystem::absolute_path_from_source(module_info.relative_path);
    return module_info;
}

}