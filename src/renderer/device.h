#ifndef FEM_DEVICE_H
#define FEM_DEVICE_H

#include <LLGL/LLGL.h>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <array>

namespace fem {

inline LLGL::RenderSystemPtr g_device = nullptr;
inline size_t g_frame_number = 0;
inline size_t g_frame_index = 0;
    
inline LLGL::Texture* g_dummy_2d_texture = nullptr;

inline LLGL::Sampler* g_linear_clamp_sampler = nullptr;

template<typename T>
T align_to(T value, T alignment)
{
    return ((value + alignment - T(1)) / alignment) * alignment;
}

enum VertexLayout {
    VERTEX_LAYOUT_DEFAULT,
    VERTEX_LAYOUT_IMGUI,
    VERTEX_LAYOUT_COUNT
};

inline std::array<LLGL::VertexShaderAttributes, VERTEX_LAYOUT_COUNT> g_vertex_layouts = {{
    {
        .inputAttribs = {
            LLGL::VertexAttribute{"position", LLGL::Format::RGB32Float, 0, 0, uint32_t(sizeof(glm::vec3)), 0}
        }
    },
    {
        .inputAttribs = {
            LLGL::VertexAttribute{"position", LLGL::Format::RG32Float, 0, uint32_t(offsetof(ImDrawVert, pos)), uint32_t(sizeof(ImDrawVert)), 0},
            LLGL::VertexAttribute{"texCoord", LLGL::Format::RG32Float, 1, uint32_t(offsetof(ImDrawVert, uv)), uint32_t(sizeof(ImDrawVert)), 0},
            LLGL::VertexAttribute{"color", LLGL::Format::RGBA8UNorm, 2, uint32_t(offsetof(ImDrawVert, col)), uint32_t(sizeof(ImDrawVert)), 0}
        }
    }
}};

}

#endif // FEM_DEVICE_H
