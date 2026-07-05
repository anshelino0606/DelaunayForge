#include "utils.h"
#include "log_categories.h"
#include "logger/logger_macros.h"
#include "core/file_system/file_system.h"

namespace fem::shaderlib {

constexpr std::string_view kSlangIRDirectory = "intermediate/slang";

#if defined(_WIN32)
constexpr std::string_view kShaderBinPath = "intermediate/dxil";
#elif defined(__APPLE__)
constexpr std::string_view kShaderBinPath = "intermediate/metal";
#endif

constexpr std::string_view kShaderBinExt = "trshaderbin";

void log_report(const LLGL::Report* report) {
    if (report->HasErrors()) {
        LOGT_ERROR(LogRenderer, report->GetText());
    }
}

void log_diagnostics(ComPtr<DiagnosticsBlob> diagnostics_blob, bool reset_blob) {
    if (diagnostics_blob) {
        const char* error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        LOGT_ERROR(LogRenderer, error_msg);

        if (reset_blob) {
            diagnostics_blob.setNull();
        }
    }
}

void create_intermediate_directories() {
    std::string_view root_path = FileSystem::get_source_path();
    std::string slang_ir_path = std::format("{}/{}", root_path, kSlangIRDirectory);
    std::string shader_bin_path = std::format("{}/{}", root_path, kShaderBinPath);

    FileSystem::create_directories(slang_ir_path);
    FileSystem::create_directories(shader_bin_path);
}

std::string get_shader_path(const std::string& relative_path) {
    return std::format("shaders/{}.slang", relative_path);
}

std::string get_cached_shader_bin_path(const std::string& module_relative_path) {
    static std::string_view root_path = FileSystem::get_source_path();
    return std::format("{}/{}/{}.{}", root_path, kShaderBinPath, module_relative_path, kShaderBinExt);
}

std::string get_and_prepare_cached_shader_bin_path(const std::string& module_relative_path) {
    std::string bin_path = get_cached_shader_bin_path(module_relative_path);    
    std::string bin_dir_only = std::filesystem::path(bin_path).parent_path().string();
    FileSystem::create_directories(bin_dir_only);
    return bin_path;
}

std::string get_cached_shader_ir_path(const std::string& module_relative_path) {
    static std::string_view root_path = FileSystem::get_source_path();
    return std::format("{}/{}/{}.slang-module", root_path, kSlangIRDirectory, module_relative_path);
}

std::string get_and_prepare_cached_shader_ir_path(const std::string& module_absolute_path) {
    static std::string shader_root_dir = FileSystem::absolute_path_from_source("shaders");
    static std::string_view root_path = FileSystem::get_source_path();
    std::string module_relative_path = FileSystem::get_relative_path(shader_root_dir, module_absolute_path);
    std::string ir_path = std::format("{}/{}/{}-module", root_path, kSlangIRDirectory, module_relative_path);
    std::string ir_dir_only = std::filesystem::path(ir_path).parent_path().string();
    FileSystem::create_directories(ir_dir_only);
    return ir_path;
}

}