#pragma once

#include "types.h"
#include "renderer/common.h"
#include <slang.h>
#include <slang-com-ptr.h>
#include <memory>
#include <string>

namespace fem::shaderlib {

class ICompilationSession {
public:
    virtual ~ICompilationSession() = default;
    virtual Session* handle() const = 0;

    Session* operator->() { return handle(); }

    LoadedModuleInfo load_module(const std::string& relative_path);
    void save_modules();
};

class CompilationSessionHandle
{
public:
    CompilationSessionHandle() = default;
    CompilationSessionHandle(std::unique_ptr<ICompilationSession> ptr)
        : ptr_(std::move(ptr)) {}

    ICompilationSession* operator->() const
    {
        return ptr_.get();
    }

    ICompilationSession& operator*() const
    {
        return *ptr_;
    }

    static CompilationSessionHandle create(GraphicsAPI api);

private:
    std::unique_ptr<ICompilationSession> ptr_ = nullptr;
};

}