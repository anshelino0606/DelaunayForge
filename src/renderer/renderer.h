#ifndef FEM_RENDERER_H
#define FEM_RENDERER_H

#include "common.h"
#include "geom/delaunay/delaunay_mesh_generator.h"
#include <functional>

#include <imgui/imgui.h>

#include "imgui_renderer.h"
#include "surface.h"
#include <LLGL/LLGL.h>

struct ImDrawData;

namespace fem {

class Window;
class Camera;
struct EditorDrawResult;
struct ViewportGridSettings;

struct RendererInitInfo {
    Window* window = nullptr;
    std::function<void()> gpu_wait_callback = nullptr;
};

struct RendererDrawInfo {
    ImDrawData* imgui_draw_data = nullptr;
    const EditorDrawResult* editor_draw_result = nullptr;
    Camera* camera = nullptr;
};

class Renderer {
public:
    bool init(const RendererInitInfo& init_info);
    void shutdown();

    void begin_frame();
    void draw(const RendererDrawInfo& draw_info);
    void draw_debug_info() const;

    ImTextureID get_viewport_texture_id() const;

private:
    bool is_initialized_ = false;
    Window* window_ = nullptr;

    std::unique_ptr<ImGuiRenderer> imgui_renderer_;

    LLGL::RenderingDebugger debugger_;

    std::shared_ptr<Surface> surface_ = nullptr;
    LLGL::SwapChain* swap_chain_ = nullptr;
    LLGL::CommandBuffer* main_cmd_ = nullptr;
    LLGL::Texture* viewport_color_texture_ = nullptr;
    LLGL::Texture* viewport_depth_texture_ = nullptr;
    LLGL::Texture* viewport_color_readback_texture_ = nullptr;
    LLGL::RenderTarget* viewport_render_target_ = nullptr;
    ImTextureID viewport_imgui_descriptor_ = 0;

    LLGL::Shader* object_vs_ = nullptr;
    LLGL::Shader* object_ps_ = nullptr;
    LLGL::PipelineState* object_pipeline_ = nullptr;
    LLGL::PipelineLayout* object_pipeline_layout_ = nullptr;
    LLGL::ResourceHeap* object_resource_heap_ = nullptr;
    LLGL::Buffer* object_vs_constant_buffer_ = nullptr;
    LLGL::Buffer* object_ps_constant_buffer_ = nullptr;

    LLGL::Shader* grid_ps_ = nullptr;
    LLGL::PipelineState* grid_pipeline_ = nullptr;
    LLGL::PipelineLayout* grid_pipeline_layout_ = nullptr;
    LLGL::ResourceHeap* grid_resource_heap_ = nullptr;
    LLGL::Buffer* grid_vs_constant_buffer_ = nullptr;
    LLGL::Buffer* grid_ps_constant_buffer_ = nullptr;
    LLGL::Buffer* grid_vertex_buffer_ = nullptr;

    LLGL::Shader* load_shader(std::string_view shader_name, LLGL::ShaderType shader_type);
    void create_object_pipeline();
    void create_grid_pipeline();
    void create_dummy_textures();
    void create_samplers();
};

}

#endif // FEM_RENDERER_H