#pragma once

#include "types.h"

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <string>

namespace fem::shaderlib::compiler {

void init();
void shutdown();
LoadedModuleInfo load_module(const std::string& relative_path);
ComPtr<EntryPoint> get_entry_point(Module* module, const char* entry_point_name);
ComPtr<ComposedProgram> compose_program(ProgramComponent* const* component_types, uint32_t component_type_count);
ComPtr<LinkedProgram> link_program(ComposedProgram* composed_program);
ComPtr<CompiledBlob> compile_shader(LinkedProgram* linked_program, uint32_t entry_point_idx);

}