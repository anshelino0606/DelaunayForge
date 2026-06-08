#pragma once

#include <LLGL/LLGL.h>
#include <slang.h>
#include <slang-com-ptr.h>

namespace fem {

void log_report(const LLGL::Report* report);
void log_diagnostics(Slang::ComPtr<slang::IBlob> diagnostics_blob, bool reset_blob = true);
LLGL::ShaderType slang_shader_type_to_llgl(slang::ProgramLayout* program_layout, uint32_t entry_point_idx = 0);

}