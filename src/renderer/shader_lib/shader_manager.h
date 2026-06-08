#pragma once

#include "shader_.h"
#include "device.h"
#include "shader_compiler.h"
#include <memory>
#include <unordered_map>

namespace fem {

class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager();

    template<VertexLayout Layout = VERTEX_LAYOUT_DEFAULT>
    Shader* get_vertex_shader(const ShaderCompileInfo& info) {
        return get_shader(info, "vertex_main", LLGL::ShaderType::Vertex, Layout);
    }

    Shader* get_pixel_shader(const ShaderCompileInfo& info);
    Shader* get_compute_shader(const ShaderCompileInfo& info);

private:
    using ShaderMap = std::unordered_map<ShaderCompileInfo, std::unique_ptr<Shader>, ShaderCompileInfoHasher>;

    ShaderCompiler shader_compiler_;
    ShaderMap shaders_;

    Shader* get_shader(
        const ShaderCompileInfo& info, 
        std::string_view default_entry_point, 
        LLGL::ShaderType type,
        VertexLayout vertex_layout = VERTEX_LAYOUT_DEFAULT
    );
};

}