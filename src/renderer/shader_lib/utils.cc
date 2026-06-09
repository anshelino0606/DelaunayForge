#include "utils.h"
#include "log_categories.h"
#include "core/macro.h"
#include "logger/logger_macros.h"

namespace fem {

void log_report(const LLGL::Report* report) {
    if (report->HasErrors()) {
        LOGT_ERROR(LogRenderer, report->GetText());
    }
}

void log_diagnostics(Slang::ComPtr<slang::IBlob> diagnostics_blob, bool reset_blob) {
    if (diagnostics_blob) {
        const char* error_msg = static_cast<const char*>(diagnostics_blob->getBufferPointer());
        LOGT_ERROR(LogRenderer, error_msg);

        if (reset_blob) {
            diagnostics_blob.setNull();
        }
    }
}

}