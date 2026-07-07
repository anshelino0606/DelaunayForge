#pragma once

#include "shader_types.h"
#include "shader_.h"
#include "renderer/device.h"

namespace fem {

constexpr const char* g_vertex_default_entry_point = "vertexMain";
constexpr const char* g_fragment_default_entry_point = "fragmentMain";

struct GraphicsShaderProgramCreateInfo : BaseShaderProgramCreateInfo {
    VertexLayout vertex_layout = VERTEX_LAYOUT_DEFAULT;

    std::string vertex_entry_point = g_vertex_default_entry_point;
    std::string fragment_entry_point = g_fragment_default_entry_point;

    bool operator==(const GraphicsShaderProgramCreateInfo& other) const = default;
};

struct GraphicsShaderProgramCreateInfoHasher {
    std::size_t operator()(const GraphicsShaderProgramCreateInfo& info) const noexcept;
};

class GraphicsShaderProgram {
public:
    GraphicsShaderProgram(const GraphicsShaderProgramCreateInfo& create_info);

    void create(const GraphicsShaderProgramCreateInfo& create_info);

    const Shader& vertex_shader() const { return vertex_shader_; }
    const Shader& fragment_shader() const { return fragment_shader_; }
    bool is_valid() const { return bool(vertex_shader_) && bool(fragment_shader_); }

private:
    Shader vertex_shader_;
    Shader fragment_shader_;
};

}
