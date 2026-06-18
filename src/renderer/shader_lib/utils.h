#pragma once

#include <LLGL/LLGL.h>
#include <slang.h>
#include <slang-com-ptr.h>

namespace fem {

void log_report(const LLGL::Report* report);
void log_diagnostics(Slang::ComPtr<slang::IBlob> diagnostics_blob, bool reset_blob = true);

}