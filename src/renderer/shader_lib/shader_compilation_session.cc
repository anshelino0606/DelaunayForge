#include "shader_compilation_session.h"
#include "shader_compilation_global_session.h"
#include "core/file_system/file_system.h"
#include <string>

namespace fem {

constexpr const char* dxil_profile_name = "sm_6_6";
constexpr const char* metal_profile_name = "metal_2_4";

slang::IGlobalSession* global_session = nullptr;

template<GraphicsAPI API>
class ShaderCompilationSessionImpl : public IShaderCompilationSession {
public:
    ShaderCompilationSessionImpl() {
        global_session = ShaderCompilationGlobalSession::instance(); 

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

        global_session->createSession(session_desc, session_.writeRef());
    }

    virtual ~ShaderCompilationSessionImpl() override {
        session_->release();
    }

    virtual slang::ISession* handle() const override { return session_; }

private:
    Slang::ComPtr<slang::ISession> session_;

    SlangProfileID get_profile_id() const {
        if constexpr (API == GraphicsAPI::D3D12) {
            return global_session->findProfile(dxil_profile_name);
        } else if constexpr (API == GraphicsAPI::METAL) {
            return global_session->findProfile(metal_profile_name);
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
ShaderCompilationSessionHandle allocate_session() {
    return ShaderCompilationSessionHandle(std::make_unique<ShaderCompilationSessionImpl<API>>());
}

ShaderCompilationSessionHandle ShaderCompilationSessionHandle::create(GraphicsAPI api) {
    switch (api) {
        case GraphicsAPI::D3D12: return allocate_session<GraphicsAPI::D3D12>();
        case GraphicsAPI::METAL: return allocate_session<GraphicsAPI::METAL>();
    }

    assert(0);
    return {};
}

}