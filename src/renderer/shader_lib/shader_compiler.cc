#include "shader_compiler.h"
#include "utils.h"
#include "core/file_system/file_system.h"
#include "log_categories.h"
#include <format>

namespace fem {

void ShaderCompiler::init() {
    slang::createGlobalSession(s_global_session_.writeRef());

#if defined(_WIN32)
    SlangProfileID profile_id = s_global_session_->findProfile("sm_6_6");

    slang::TargetDesc target_desc = {
        .format = SLANG_DXIL,
        .profile = profile_id
    };
#elif defined(__APPLE__)
    SlangProfileID profile_id = global_session_->findProfile("metal_2_4");

    slang::TargetDesc target_desc = {
        .format = SLANG_METAL,
        .profile = profile_id
    };
#endif

    slang::SessionDesc session_desc = {
        .targets = &target_desc,
        .targetCount = 1,
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR
    };

    s_global_session_->createSession(session_desc, s_session_.writeRef());
}

void ShaderCompiler::shutdown() {
    s_session_->Release();
    s_global_session_->Release();
    slang::shutdown();
}

Slang::ComPtr<slang::IModule> ShaderCompiler::load_module(const std::string& relative_path) {
    ShaderModuleLoadInfo load_info = module_load_info_from_relative_path(relative_path);

    std::string shader_content;
    FileSystem::read(load_info.absolute_path, shader_content);

    Slang::ComPtr<slang::IBlob> diagnostics_blob;

    Slang::ComPtr<slang::IModule> slang_module;
    slang_module = s_session_->loadModuleFromSourceString(
        load_info.name.c_str(), 
        load_info.relative_path.c_str(), 
        shader_content.c_str(),
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

    SlangResult result = s_session_->createCompositeComponentType(
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
    module_info.absolute_path = FileSystem::get_absolute_path(FileSystem::get_source_path(), module_info.relative_path);
    return module_info;
}

}