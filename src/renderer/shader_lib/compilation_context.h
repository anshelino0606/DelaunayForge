#pragma once

#include "types.h"
#include "renderer/rhi/shader_types.h"
#include <slang-com-ptr.h>
#include <slang.h>

namespace fem::shaderlib {

struct CompilationResult {
    const void* binary = nullptr;
    size_t size = 0;
};

class CompilationContext {
public:
    CompilationContext(const BaseShaderProgramCreateInfo& create_info);

    void add_entry_point(const std::string& entry_point_name);
    void compile();

    CompilationResult get_compiled_shader(uint32_t entry_point_idx) const;

private:
    std::string relative_path_;
    LoadedModuleInfo loaded_module_info_;
    ComPtr<LinkedProgram> linked_program_;
    std::vector<ComPtr<EntryPoint>> entry_points_;
    std::vector<ComPtr<CompiledBlob>> compiled_shaders_;

    void link_program();
    void compile_and_cache_shader(uint32_t entry_point_idx);
    void load_shader_from_cache(uint32_t entry_point_idx);
    std::string get_shader_bin_relative_path(uint32_t entry_point_idx);
};

}