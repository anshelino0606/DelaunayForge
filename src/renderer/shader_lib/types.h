#pragma once

#include <slang.h>
#include <slang-com-ptr.h>

namespace fem::shaderlib {

template<typename T>
using ComPtr = Slang::ComPtr<T>;

using GlobalSession = slang::IGlobalSession;
using Session = slang::ISession;

using Module = slang::IModule;
using EntryPoint = slang::IEntryPoint;
using EntryPointRefl = slang::EntryPointReflection;
using ProgramComponent = slang::IComponentType;
using ComposedProgram = slang::IComponentType;
using LinkedProgram = slang::IComponentType;
using CompiledBlob = slang::IBlob;
using IRBlob = slang::IBlob;
using DiagnosticsBlob = slang::IBlob;
using ProgramLayout = slang::ProgramLayout;

struct LoadedModuleInfo {
    ComPtr<Module> module = nullptr;
    bool is_cache_valid = false;  
};

}