#pragma once

#include <slang.h>
#include <slang-com-ptr.h>

namespace fem {

class ShaderCompilationGlobalSession {
public:
    static slang::IGlobalSession* instance()
    {
        static ShaderCompilationGlobalSession singleton;
        return singleton.session_.get();
    }

    ShaderCompilationGlobalSession(const ShaderCompilationGlobalSession&) = delete;
    ShaderCompilationGlobalSession& operator=(const ShaderCompilationGlobalSession&) = delete;
    ShaderCompilationGlobalSession(ShaderCompilationGlobalSession&&) = delete;
    ShaderCompilationGlobalSession& operator=(ShaderCompilationGlobalSession&&) = delete;

private:
    ShaderCompilationGlobalSession()
    {
        slang::createGlobalSession(session_.writeRef());
    }

    Slang::ComPtr<slang::IGlobalSession> session_;
};

}