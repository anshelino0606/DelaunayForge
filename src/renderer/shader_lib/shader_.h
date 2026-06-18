#pragma once

#include <LLGL/LLGL.h>
#include <slang.h>
#include <string_view>

namespace fem {

class Shader {
public:
    struct InitInfo {
        const void* data = nullptr;
        size_t data_size = 0;
        std::string_view debug_name;
        std::string_view entry_point; 
        LLGL::ShaderType type = LLGL::ShaderType::Undefined;
        const LLGL::VertexShaderAttributes* vertex_attribs = nullptr; 
    };

    Shader() = default;
    Shader(const InitInfo& info);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    LLGL::Shader* handle() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

    void destroy();
    void create(const InitInfo& info);

private:
    LLGL::Shader* handle_ = nullptr;
};

}