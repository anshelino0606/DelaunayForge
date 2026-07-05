#include "compiler.h"
#include "utils.h"
#include "log_categories.h"

namespace fem::shaderlib::compiler {

static struct Compiler {
    CompilationSessionHandle session;

    void init() {
#if defined(_WIN32)
        session = CompilationSessionHandle::create(GraphicsAPI::D3D12);
#elif defined(__APPLE__)
        session = CompilationSessionHandle::create(GraphicsAPI::METAL);
#endif
    }

    void shutdown() {
        session->save_modules();
    }
} s_compiler;

void init() {
    s_compiler.init();
    create_intermediate_directories();
}

void shutdown() {
    s_compiler.shutdown();
}

LoadedModuleInfo load_module(const std::string& relative_path) {
    return s_compiler.session->load_module(relative_path);
}

ComPtr<EntryPoint> get_entry_point(Module* module, const char* entry_point_name) {
    ComPtr<EntryPoint> entry_point;
    module->findEntryPointByName(entry_point_name, entry_point.writeRef());
    if (!entry_point) {
        LOGT_ERROR(LogRenderer, "Failed to find entry point [%s] for shader [%s]!", entry_point_name, module->getFilePath());
    }
    return entry_point;
}

ComPtr<ComposedProgram> compose_program(ProgramComponent* const* component_types, uint32_t component_type_count) {
    ComPtr<DiagnosticsBlob> diagnostics_blob;
    ComPtr<ComposedProgram> composed_program;

    SlangResult result = s_compiler.session->handle()->createCompositeComponentType(
        component_types, 
        component_type_count, 
        composed_program.writeRef(), 
        diagnostics_blob.writeRef()
    );
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);
    return composed_program;
}

ComPtr<LinkedProgram> link_program(ComposedProgram* composed_program) {
    ComPtr<DiagnosticsBlob> diagnostics_blob;
    ComPtr<LinkedProgram> linked_program;

    SlangResult result = composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);
    return linked_program;
}

ComPtr<CompiledBlob> compile_shader(LinkedProgram* linked_program, uint32_t entry_point_idx) {
    ComPtr<DiagnosticsBlob> diagnostics_blob;
    ComPtr<CompiledBlob> compiled_code;

    SlangResult result = linked_program->getEntryPointCode(entry_point_idx, 0, compiled_code.writeRef(), diagnostics_blob.writeRef());
    log_diagnostics(diagnostics_blob);
    SLANG_RETURN_NULL_ON_FAIL(result);
    return compiled_code;
}

}