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

LLGL::ShaderType slang_shader_type_to_llgl(slang::ProgramLayout* program_layout, uint32_t entry_point_idx) {
    slang::EntryPointLayout* entry_point_layout = program_layout->getEntryPointByIndex(entry_point_idx);

    switch (entry_point_layout->getStage()) {
        case SLANG_STAGE_VERTEX:
            return LLGL::ShaderType::Vertex;
        case SLANG_STAGE_FRAGMENT:
            return LLGL::ShaderType::Fragment;
        case SLANG_STAGE_COMPUTE:
            return LLGL::ShaderType::Compute;
        case SLANG_STAGE_GEOMETRY:
            return LLGL::ShaderType::Geometry;
        case SLANG_STAGE_HULL:
            return LLGL::ShaderType::TessControl;
        case SLANG_STAGE_DOMAIN:
            return LLGL::ShaderType::TessEvaluation;
        case SLANG_STAGE_MESH:
            return LLGL::ShaderType::Mesh;
        case SLANG_STAGE_AMPLIFICATION:
            return LLGL::ShaderType::Task;
        default:
            assert(0);
            return LLGL::ShaderType::Undefined;
    }
}

}