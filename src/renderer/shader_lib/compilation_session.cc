#include "compilation_session.h"
#include "global_compilation_session.h"
#include "utils.h"
#include "vector_blob.h"
#include "core/file_system/file_system.h"
#include "log_categories.h"
#include <string>

namespace fem::shaderlib {

constexpr const char* dxil_profile_name = "sm_6_6";
constexpr const char* metal_profile_name = "metal_2_4";

LoadedModuleInfo ICompilationSession::load_module(const std::string& relative_path) {
    std::vector<uint8_t> blob_data;
    std::string ir_path = get_cached_shader_ir_path(relative_path);
    std::string shader_code_path = FileSystem::absolute_path_from_source(get_shader_path(relative_path));
    FileSystem::read(ir_path, blob_data);

    Session* session = handle();

    ComPtr<IRBlob> ir_blob;
    ir_blob.attach(new VectorBlob(std::move(blob_data)));

    Slang::ComPtr<slang::IBlob> diagnostics_blob;

    LoadedModuleInfo module_info;

    if (session->isBinaryModuleUpToDate(relative_path.c_str(), ir_blob)) {
        module_info.module = session->loadModuleFromIRBlob(
            relative_path.c_str(), 
            shader_code_path.c_str(),
            ir_blob,
            diagnostics_blob.writeRef()
        );
        log_diagnostics(diagnostics_blob);

        if (!module_info.module) {
            LOGT_ERROR(LogRenderer, "Failed to load shader cache for [%s]", relative_path.c_str());
        } else {
            module_info.is_cache_valid = true;
        }
    } else {
        module_info.module = session->loadModule(
            relative_path.c_str(),
            diagnostics_blob.writeRef()
        );
        log_diagnostics(diagnostics_blob);
        if (!module_info.module) {
            LOGT_ERROR(LogRenderer, "Failed to load slang module [%s]", get_shader_path(relative_path).c_str());
        }
    }

    return module_info;
}

void ICompilationSession::save_modules() {
    Session* session_handle = handle();
    int32_t loaded_module_count = session_handle->getLoadedModuleCount();

    for (int32_t i = 0; i != loaded_module_count; ++i) {
        slang::IModule* module = session_handle->getLoadedModule(i);
        std::string module_ir_path = get_and_prepare_cached_shader_ir_path(module->getFilePath());
        Slang::ComPtr<slang::IBlob> serialized_module;
        if (SLANG_FAILED(module->serialize(serialized_module.writeRef()))) {
            LOGT_ERROR(LogRenderer, "Failed to serialize slang module [%s]", module_ir_path.c_str());
            continue;
        }
        const uint8_t* data = static_cast<const uint8_t*>(serialized_module->getBufferPointer());
        FileSystem::write(module_ir_path, data, serialized_module->getBufferSize());
    }
}

template<GraphicsAPI API>
class CompilationSessionImpl : public ICompilationSession {
public:
    CompilationSessionImpl() {
        slang::TargetDesc target_desc = {
            .format = get_compile_target(),
            .profile = get_profile_id()
        };

        const std::vector<const char*>& search_paths = get_search_paths();

        slang::SessionDesc session_desc = {
            .targets = &target_desc,
            .targetCount = 1,
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .searchPaths = search_paths.data(),
            .searchPathCount = static_cast<SlangInt>(search_paths.size())
        };

        global_session()->createSession(session_desc, session_.writeRef());
    }

    virtual slang::ISession* handle() const override { return session_; }

private:
    Slang::ComPtr<slang::ISession> session_;

    SlangProfileID get_profile_id() const {
        if constexpr (API == GraphicsAPI::D3D12) {
            return global_session()->findProfile(dxil_profile_name);
        } else if constexpr (API == GraphicsAPI::METAL) {
            return global_session()->findProfile(metal_profile_name);
        } else {
            static_assert(API == GraphicsAPI::D3D12 || API == GraphicsAPI::METAL);
        }
    }

    SlangCompileTarget get_compile_target() const {
        if constexpr (API == GraphicsAPI::D3D12) {
            return SLANG_DXIL;
        } else if constexpr (API == GraphicsAPI::METAL) {
            return SLANG_METAL;
        } else {
            static_assert(API == GraphicsAPI::D3D12 || API == GraphicsAPI::METAL);
        }
    }

    const std::vector<const char*>& get_search_paths() const {
        static std::string core_shader_directory = FileSystem::absolute_path_from_source("shaders");
        static std::vector<const char*> search_paths = {
            core_shader_directory.c_str()
        };
        return search_paths;
    }
};

template<GraphicsAPI API>
CompilationSessionHandle allocate_session() {
    return CompilationSessionHandle(std::make_unique<CompilationSessionImpl<API>>());
}

CompilationSessionHandle CompilationSessionHandle::create(GraphicsAPI api) {
    switch (api) {
        case GraphicsAPI::D3D12: return allocate_session<GraphicsAPI::D3D12>();
        case GraphicsAPI::METAL: return allocate_session<GraphicsAPI::METAL>();
    }

    assert(0);
    return {};
}

}