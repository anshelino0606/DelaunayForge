#pragma once

#include "types.h"
#include <LLGL/LLGL.h>
#include <slang.h>
#include <slang-com-ptr.h>

namespace fem::shaderlib {

void log_report(const LLGL::Report* report);
void log_diagnostics(ComPtr<DiagnosticsBlob> diagnostics_blob, bool reset_blob = true);

void create_intermediate_directories();
std::string get_shader_path(const std::string& relative_path);
std::string get_cached_shader_ir_path(const std::string& module_relative_path);
std::string get_and_prepare_cached_shader_ir_path(const std::string& module_absolute_path);
}