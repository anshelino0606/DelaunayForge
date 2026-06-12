#ifndef FEM_IMGUI_RENDERER_H
#define FEM_IMGUI_RENDERER_H

#include <LLGL/LLGL.h>
#include <imgui/imgui.h>

#include <array>

struct ImDrawData;

namespace fem {

class GraphicsShaderProgram;
class ShaderManager;

struct ImGuiRendererInitInfo {
    LLGL::SwapChain* swap_chain = nullptr;
    ShaderManager* shader_manager = nullptr;
};

struct ImGuiRendererDrawInfo {
    ImDrawData* draw_data = nullptr;
    LLGL::CommandBuffer* cmd = nullptr;
};

class ImGuiRenderer{
public:
    bool init(const ImGuiRendererInitInfo& init_info);
    void shutdown();

    void draw(const ImGuiRendererDrawInfo& draw_info);

    ImTextureID allocate_imgui_descriptor(LLGL::Texture* texture);
    void update_descriptor(uint64_t descriptor_idx, const std::vector<LLGL::ResourceViewDescriptor>& descriptors);

private:
    bool is_initialized_ = false;

    ShaderManager* shader_manager_ = nullptr;;

    LLGL::Format index_format_ = LLGL::Format::R16UInt;

    static constexpr uint32_t kMaxTextures = 16;
    std::array<LLGL::Texture*, kMaxTextures> textures_{};

    LLGL::PipelineState* pipeline_ = nullptr;
    LLGL::PipelineLayout* pipeline_layout_ = nullptr;
    LLGL::Texture* font_texture_ = nullptr;
    GraphicsShaderProgram* shader_program_ = nullptr;
    LLGL::Buffer* vs_constant_buffer_ = nullptr;
    std::vector<LLGL::Buffer*> vertex_buffers_;
    std::vector<LLGL::Buffer*> index_buffers_;

    uint64_t next_texture_idx_ = 0;

    void create_pipeline(const LLGL::RenderPass* render_pass);
    void create_resources(LLGL::SwapChain* swap_chain);
    void create_font_texture();
    LLGL::Shader* load_shader(std::string_view shader_name, LLGL::ShaderType shader_type);

    LLGL::Buffer* get_vertex_buffer(uint32_t buffer_size);
    LLGL::Buffer* get_index_buffer(uint32_t buffer_size);
};

}

#endif // FEM_IMGUI_RENDERER_H