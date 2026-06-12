#include "shader_.h"
#include "renderer/device.h"

#include <slang.h>
#include <slang-com-ptr.h>

namespace fem {

Shader::Shader(const InitInfo& info) {
    create(info);
}

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept {
    handle_ = other.handle_;
    other.handle_ = nullptr;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (handle_) g_device->Release(*handle_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

void Shader::destroy() {
    if (handle_) {
        g_device->Release(*handle_);
        handle_ = nullptr;
    }
}

void Shader::create(const InitInfo& info) {
    destroy();

    LLGL::ShaderDescriptor shader_desc;
    shader_desc.type = info.type;
    shader_desc.debugName = info.debug_name.data();
    shader_desc.entryPoint = info.entry_point.data();
    shader_desc.source = static_cast<const char*>(info.data);
    shader_desc.sourceSize = info.data_size;

#if defined(_WIN32)
    shader_desc.sourceType = LLGL::ShaderSourceType::BinaryBuffer;
#elif defined(__APPLE__)
    shader_desc.sourceType = LLGL::ShaderSourceType::CodeString;
#endif
    
    if (shader_desc.type == LLGL::ShaderType::Vertex && info.vertex_attribs) {
        shader_desc.vertex = *info.vertex_attribs;
    }

    handle_ = g_device->CreateShader(shader_desc);
}

} // namespace fem
