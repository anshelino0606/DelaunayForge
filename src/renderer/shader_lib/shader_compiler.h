#pragma once

#include "shader_types.h"

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

namespace fem {

class ShaderCompiler {
public:
    ShaderCompiler();
    ~ShaderCompiler();

    Slang::ComPtr<slang::IBlob> compile(const ShaderCompileInfo& info);

private:
    Slang::ComPtr<slang::IGlobalSession> global_session_;
    Slang::ComPtr<slang::ISession> session_;
};

}