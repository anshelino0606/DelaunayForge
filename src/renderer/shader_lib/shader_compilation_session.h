#pragma once

#include "renderer/common.h"
#include <slang.h>
#include <slang-com-ptr.h>
#include <memory>

namespace fem {

class IShaderCompilationSession {
public:
    virtual ~IShaderCompilationSession() = default;
    virtual slang::ISession* handle() const = 0;

    slang::ISession* operator->() { return handle(); }
};

class ShaderCompilationSessionHandle
{
public:
    ShaderCompilationSessionHandle() = default;
    ShaderCompilationSessionHandle(std::unique_ptr<IShaderCompilationSession> ptr)
        : ptr_(std::move(ptr)) {}

    IShaderCompilationSession* operator->() const
    {
        return ptr_.get();
    }

    IShaderCompilationSession& operator*() const
    {
        return *ptr_;
    }

    static ShaderCompilationSessionHandle create(GraphicsAPI api);

private:
    std::unique_ptr<IShaderCompilationSession> ptr_ = nullptr;
};

}