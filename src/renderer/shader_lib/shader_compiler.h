#pragma once

#include "shader_compilation_session.h"

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <string>

namespace fem {

class ShaderCompiler {
public:
    static void init();
    static void shutdown();

    static Slang::ComPtr<slang::IModule> load_module(const std::string& relative_path);
    static Slang::ComPtr<slang::IEntryPoint> get_entry_point(slang::IModule* module, const char* entry_point_name);
    static Slang::ComPtr<slang::IComponentType> compose_program(slang::IComponentType* const* component_types, uint32_t component_type_count);
    static Slang::ComPtr<slang::IComponentType> link_program(slang::IComponentType* composed_program);
    static Slang::ComPtr<slang::IBlob> compile_shader(slang::IComponentType* linked_program, uint32_t entry_point_idx);

private:
    inline static ShaderCompilationSessionHandle s_session_;

    struct ShaderModuleLoadInfo {
        std::string name;
        std::string relative_path;
        std::string absolute_path;
    };

    static ShaderModuleLoadInfo module_load_info_from_relative_path(const std::string& relative_path);
};

}