#include "shader_compiler.h"
#include "utils.h"
#include "core/file_system/file_system.h"
#include "log_categories.h"
#include <array>
#include <format>

namespace fem {

ShaderCompiler::ShaderCompiler() {
    slang::createGlobalSession(global_session_.writeRef());

#if defined(_WIN32)
    SlangProfileID profile_id = global_session_->findProfile("sm_6_6");

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

    global_session_->createSession(session_desc, session_.writeRef());
}

ShaderCompiler::~ShaderCompiler() {
    session_->Release();
    global_session_->Release();
    slang::shutdown();
}

Slang::ComPtr<slang::IBlob> ShaderCompiler::compile(const ShaderCompileInfo& info) {
    if (info.entry_point == std::nullopt) {
        LOGT_ERROR(LogRenderer, "Invalid entry point for [%s] shader", info.relative_path.c_str());
    }
    
    std::string shader_module_name = FileSystem::get_file_name(info.relative_path);
    std::string shader_full_rel_path = std::format("shaders/{}.slang", info.relative_path);
    std::string shader_path = FileSystem::get_absolute_path(FileSystem::get_source_path(), shader_full_rel_path);

    std::string shader_content;
    FileSystem::read(shader_path, shader_content);

    Slang::ComPtr<slang::IBlob> diagnostics_blob;

    Slang::ComPtr<slang::IModule> slang_module;
    slang_module = session_->loadModuleFromSourceString(
        shader_module_name.data(), 
        shader_full_rel_path.c_str(), 
        shader_content.c_str(),
        diagnostics_blob.writeRef()
    );
    log_diagnostics(diagnostics_blob);
    if (!slang_module) {
        LOGT_ERROR(LogRenderer, "Failed to load slang module [%s]", shader_full_rel_path.c_str());
        return nullptr;
    }

    const char* entry_point_name = info.entry_point->c_str();
    Slang::ComPtr<slang::IEntryPoint> entry_point;
    slang_module->findEntryPointByName(entry_point_name, entry_point.writeRef());
    if (!entry_point) {
        LOGT_ERROR(LogRenderer, "Failed to find entry point [%s] for shader [%s]!", entry_point_name, shader_full_rel_path.c_str());
        return nullptr;
    }

    std::array<slang::IComponentType*, 2> component_types = {
        slang_module,
        entry_point
    };

    Slang::ComPtr<slang::IComponentType> composed_program;
    SlangResult result = session_->createCompositeComponentType(
        component_types.data(), 
        component_types.size(), 
        composed_program.writeRef(), 
        diagnostics_blob.writeRef()
    );
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);

    Slang::ComPtr<slang::IComponentType> linked_program;
    result = composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);

    Slang::ComPtr<slang::IBlob> compiled_code;
    result = linked_program->getEntryPointCode(0, 0, compiled_code.writeRef(), diagnostics_blob.writeRef());
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);

    return compiled_code;
}

}