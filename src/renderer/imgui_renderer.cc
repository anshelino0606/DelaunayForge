#include "imgui_renderer.h"
#include "log_categories.h"
#include "core/file_system/file_system.h"
#include "renderer/device.h"

#include <imgui/imgui.h>
#include <format>
#include <cstdint>
#include <glm/glm.hpp>

namespace fem {

constexpr uint32_t TEXTURE_ARRAY_SIZE = 16;
constexpr uint32_t BUFFER_ALIGNMENT = 256;
constexpr uint32_t INIT_VERTEX_COUNT = 4096;

const std::vector<LLGL::VertexAttribute> g_vertex_attribs = {
    LLGL::VertexAttribute{"position", LLGL::Format::RG32Float, 0, offsetof(ImDrawVert, pos), sizeof(ImDrawVert), 0},
    LLGL::VertexAttribute{"texCoord", LLGL::Format::RG32Float, 1, offsetof(ImDrawVert, uv), sizeof(ImDrawVert), 0},
#if defined(_WIN32)
    LLGL::VertexAttribute{"color", LLGL::Format::RGBA8UNorm, 2, offsetof(ImDrawVert, col), sizeof(ImDrawVert), 0}
#elif defined(__APPLE__)
    LLGL::VertexAttribute{"color", LLGL::Format::RGBA8UInt, 2, offsetof(ImDrawVert, col), sizeof(ImDrawVert), 0}
#endif
};

bool ImGuiRenderer::init(const ImGuiRendererInitInfo& init_info) {
    if (is_initialized_) {
        return true;
    }

    if (!init_info.swap_chain) {
        LOGT_ERROR(LogRenderer, "ImGuiRenderer::init(): swap_chain is null");
        return false;
    }

    if constexpr (sizeof(ImDrawIdx) == 2) {
        index_format_ = LLGL::Format::R16UInt;
    } else {
        index_format_ = LLGL::Format::R32UInt;
    }

    textures_.fill(nullptr);

    create_pipeline(init_info.swap_chain->GetRenderPass());
    create_resources(init_info.swap_chain);
    create_font_texture();

    is_initialized_ = true;

    return true;
}

void ImGuiRenderer::shutdown() {
    g_device->Release(*pipeline_);
    g_device->Release(*pipeline_layout_);
    g_device->Release(*font_texture_);
    g_device->Release(*vertex_shader_);
    g_device->Release(*pixel_shader_);
    g_device->Release(*vs_constant_buffer_);
    
    for (LLGL::Buffer* buffer : vertex_buffers_) {
        g_device->Release(*buffer);
    }

    for (LLGL::Buffer* buffer : index_buffers_) {
        g_device->Release(*buffer);
    }

    is_initialized_ = false;
}

void ImGuiRenderer::draw(const ImGuiRendererDrawInfo& draw_info) {
    static std::vector<ImDrawVert> vertices;
    static std::vector<ImDrawIdx> indices;

    ImDrawData* draw_data = draw_info.draw_data;

    static uint64_t s_frame_counter = 0;
    const uint64_t frame_no = s_frame_counter++;
    const bool log_this_frame = (frame_no < 10u) || ((frame_no % 120u) == 0u);

    if (!draw_data) {
        if (log_this_frame) {
            LOGT_WARN(LogRenderer, "draw_data is null");
        }
        return;
    }
    if (!draw_info.cmd) {
        if (log_this_frame) {
            LOGT_WARN(LogRenderer, "cmd is null");
        }
        return;
    }
    if (!pipeline_ || !pipeline_layout_ || !vs_constant_buffer_) {
        if (log_this_frame) {
            LOGT_ERROR(LogRenderer, "missing GPU resources (pipeline=%p layout=%p vsCB=%p)",
                       (void*)pipeline_, (void*)pipeline_layout_, (void*)vs_constant_buffer_);
        }
        return;
    }
    if (!g_linear_clamp_sampler) {
        if (log_this_frame) {
            LOGT_ERROR(LogRenderer, "g_linear_clamp_sampler is null");
        }
        return;
    }

    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return; 

    // LLGL expects buffer sizes to be compatible with the bound vertex format stride.
    // Aligning to BUFFER_ALIGNMENT can produce sizes that are not divisible by
    // sizeof(ImDrawVert) (20 bytes), which triggers warnings on some backends.
    const uint32_t vertex_size = uint32_t(draw_data->TotalVtxCount * sizeof(ImDrawVert));
    const uint32_t index_size  = uint32_t(draw_data->TotalIdxCount * sizeof(ImDrawIdx));

    LLGL::Buffer* vertex_buffer = get_vertex_buffer(vertex_size);
    LLGL::Buffer* index_buffer = get_index_buffer(index_size);

    vertices.clear();
    indices.clear();

    for (uint32_t i = 0; i != draw_data->CmdListsCount; ++i) {
        const ImDrawList* draw_list = draw_data->CmdLists[i];

        for (uint32_t j = 0; j != draw_list->VtxBuffer.Size; ++j) {
            vertices.push_back(draw_list->VtxBuffer[j]);
        }

        for (uint32_t j = 0; j != draw_list->IdxBuffer.Size; ++j) {
            indices.push_back(draw_list->IdxBuffer[j]);
        }
    }

    g_device->WriteBuffer(*vertex_buffer, 0, vertices.data(), vertices.size() * sizeof(ImDrawVert));
    g_device->WriteBuffer(*index_buffer, 0, indices.data(), indices.size() * sizeof(ImDrawIdx));

    glm::vec4 vs_ui_data;
    vs_ui_data.x = 2.0f / draw_data->DisplaySize.x;
    vs_ui_data.y = 2.0f / draw_data->DisplaySize.y;
    vs_ui_data.z = -1.0f - draw_data->DisplayPos.x * vs_ui_data.x;
    vs_ui_data.w = -1.0f - draw_data->DisplayPos.y * vs_ui_data.y;

    g_device->WriteBuffer(*vs_constant_buffer_, 0, &vs_ui_data, sizeof(glm::vec4));

    LLGL::Viewport main_viewport;
    main_viewport.width = static_cast<float>(fb_width);
    main_viewport.height = static_cast<float>(fb_height);

    LLGL::CommandBuffer* cmd = draw_info.cmd;

    cmd->SetPipelineState(*pipeline_);
    cmd->SetResource(0, *vs_constant_buffer_);
    cmd->SetResource(2, *g_linear_clamp_sampler);
    cmd->SetVertexBuffer(*vertex_buffer);
    cmd->SetIndexBuffer(*index_buffer, index_format_);
    cmd->SetViewport(main_viewport);

    uint32_t global_vertex_offset = 0;
    uint32_t global_index_offset = 0;

    glm::vec2 clipOff(draw_data->DisplayPos.x, draw_data->DisplayPos.y);
    glm::vec2 clipScale(draw_data->FramebufferScale.x, draw_data->FramebufferScale.y);

    for (uint32_t i = 0; i != draw_data->CmdListsCount; ++i) {
        const ImDrawList* draw_list = draw_data->CmdLists[i];
        for (uint32_t j = 0; j != draw_list->CmdBuffer.Size; ++j)
        {
            const ImDrawCmd* im_cmd = &draw_list->CmdBuffer[j];
            
            glm::vec2 clipMin((im_cmd->ClipRect.x - clipOff.x) * clipScale.x, (im_cmd->ClipRect.y - clipOff.y) * clipScale.y);
            glm::vec2 clipMax((im_cmd->ClipRect.z - clipOff.x) * clipScale.x, (im_cmd->ClipRect.w - clipOff.y) * clipScale.y);

            if (clipMin.x < 0.0f) { clipMin.x = 0.0f; }
            if (clipMin.y < 0.0f) { clipMin.y = 0.0f; }
            if (clipMax.x > fb_width) { clipMax.x = (float)fb_width; }
            if (clipMax.y > fb_height) { clipMax.y = (float)fb_height; }
            if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                continue;

            LLGL::Scissor scissor;
            scissor.x = static_cast<int32_t>(clipMin.x);
            scissor.y = static_cast<int32_t>(clipMin.y);
            scissor.width = static_cast<int32_t>(clipMax.x - clipMin.x);
            scissor.height = static_cast<int32_t>(clipMax.y - clipMin.y);

            const uint64_t tex_id = static_cast<uint64_t>(im_cmd->GetTexID());
            const uint32_t tex_idx = static_cast<uint32_t>(tex_id);


            if (tex_idx >= kMaxTextures) {
                if (log_this_frame) {
                    LOGT_WARN(LogRenderer, "texture index %u out of range (max %u)", tex_idx, kMaxTextures);
                }
            }

            LLGL::Texture* texture = nullptr;
            if (tex_idx < kMaxTextures) {
                texture = textures_[tex_idx];
            }
            if (!texture) {
                texture = font_texture_;
            }
            cmd->SetResource(1, *texture);

            cmd->SetScissor(scissor);
            cmd->DrawIndexed(
                im_cmd->ElemCount, 
                im_cmd->IdxOffset + global_index_offset,
                im_cmd->VtxOffset + global_vertex_offset
            );
        }

        global_vertex_offset += draw_list->VtxBuffer.Size;
        global_index_offset += draw_list->IdxBuffer.Size;
    }

    LLGL::Scissor scissor;
    scissor.x = 0;
    scissor.y = 0;
    scissor.width = fb_width;
    scissor.height = fb_height;
    cmd->SetScissor(scissor);
}

ImTextureID ImGuiRenderer::allocate_imgui_descriptor(LLGL::Texture* texture) {
    if (next_texture_idx_ >= kMaxTextures - 1) {
        LOGT_ERROR(LogRenderer, "ImGui can't use more than {} textures!", kMaxTextures);
        return 0;
    }

    ++next_texture_idx_;
    textures_[next_texture_idx_] = texture;

    LOGT_DEBUG(LogRenderer, "idx=%llu tex=%p", (unsigned long long)next_texture_idx_, (void*)texture);

    return static_cast<ImTextureID>(next_texture_idx_);
}

void ImGuiRenderer::update_descriptor(uint64_t descriptor_idx, const std::vector<LLGL::ResourceViewDescriptor>& descriptors) {
    (void)descriptor_idx;
    (void)descriptors;
}

void ImGuiRenderer::create_pipeline(const LLGL::RenderPass* render_pass) {
    constexpr std::string_view vertex_shader_path = "imgui_vs";
    constexpr std::string_view pixel_shader_path = "imgui_ps";

    vertex_shader_ = load_shader(vertex_shader_path, LLGL::ShaderType::Vertex);
    pixel_shader_ = load_shader(pixel_shader_path, LLGL::ShaderType::Fragment);

    LLGL::PipelineLayoutDescriptor layout_desc;
    layout_desc.bindings = {
    #if defined(__APPLE__)
        { LLGL::ResourceType::Buffer,  LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::VertexStage,   1 },
    #else
        { LLGL::ResourceType::Buffer,  LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::VertexStage,   0 }, // b0
    #endif
        { LLGL::ResourceType::Texture, LLGL::BindFlags::Sampled,        LLGL::StageFlags::FragmentStage, 0 }, // t0
        { LLGL::ResourceType::Sampler, 0,                               LLGL::StageFlags::FragmentStage, 0 }  // s0
    };

    pipeline_layout_ = g_device->CreatePipelineLayout(layout_desc);

    LLGL::GraphicsPipelineDescriptor pipeline_desc {
        .pipelineLayout = pipeline_layout_,
        .renderPass = const_cast<LLGL::RenderPass*>(render_pass),
        .vertexShader = vertex_shader_,
        .fragmentShader = pixel_shader_,
        .indexFormat = index_format_,
        .primitiveTopology = LLGL::PrimitiveTopology::TriangleList,
        .depth = {
            .testEnabled = false,
            .writeEnabled = false,
        },
        .rasterizer = {
            .polygonMode = LLGL::PolygonMode::Fill,
            .cullMode = LLGL::CullMode::Disabled,
            .scissorTestEnabled = true,
            .multiSampleEnabled = false,
        },
        .blend = {
            .targets = {
                {
                    .blendEnabled = true,
                    .srcColor = LLGL::BlendOp::SrcAlpha,
                    .dstColor = LLGL::BlendOp::InvSrcAlpha,
                    .colorArithmetic = LLGL::BlendArithmetic::Add,
                    .srcAlpha = LLGL::BlendOp::One,
                    .dstAlpha = LLGL::BlendOp::InvSrcAlpha,
                    .alphaArithmetic = LLGL::BlendArithmetic::Add,
                    .colorMask = LLGL::ColorMaskFlags::All
                }
            }
        }
    };

    pipeline_ = g_device->CreatePipelineState(pipeline_desc);

    if (pipeline_ && pipeline_->GetReport()) {
        const LLGL::Report* report = pipeline_->GetReport();

        if (report->HasErrors()) {
            LOGT_ERROR(LogRenderer, "%s", report->GetText());
        }
    }
}

void ImGuiRenderer::create_resources(LLGL::SwapChain* swap_chain) {
    LLGL::BufferDescriptor vs_constant_buffer_desc;
    vs_constant_buffer_desc.size = BUFFER_ALIGNMENT;
    vs_constant_buffer_desc.bindFlags = LLGL::BindFlags::ConstantBuffer;
    vs_constant_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;

    vs_constant_buffer_ = g_device->CreateBuffer(vs_constant_buffer_desc);

    vertex_buffers_.reserve(swap_chain->GetNumSwapBuffers());
    index_buffers_.reserve(swap_chain->GetNumSwapBuffers());

    LLGL::BufferDescriptor vertex_buffer_desc;
    vertex_buffer_desc.size = INIT_VERTEX_COUNT * sizeof(ImDrawVert);
    vertex_buffer_desc.bindFlags = LLGL::BindFlags::VertexBuffer;
    vertex_buffer_desc.vertexAttribs = g_vertex_attribs;
    vertex_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;

    LLGL::BufferDescriptor index_buffer_desc;
    index_buffer_desc.size = INIT_VERTEX_COUNT * sizeof(ImDrawIdx);
    index_buffer_desc.bindFlags = LLGL::BindFlags::IndexBuffer;
    index_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
    index_buffer_desc.format = index_format_;

    for (uint32_t i = 0; i != swap_chain->GetNumSwapBuffers(); ++i) {
        vertex_buffers_.push_back(g_device->CreateBuffer(vertex_buffer_desc));
        index_buffers_.push_back(g_device->CreateBuffer(index_buffer_desc));
    }

    (void)swap_chain;
}

void ImGuiRenderer::create_font_texture() {
    ImGuiIO& io = ImGui::GetIO();
    
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    const uint32_t pixelsByteSize = width * height * 4;

    LLGL::TextureDescriptor texture_desc;
    texture_desc.type = LLGL::TextureType::Texture2D;
    texture_desc.extent = { (uint32_t)width, (uint32_t)height, 1 };
    texture_desc.format = LLGL::Format::RGBA8UNorm;
    texture_desc.bindFlags = LLGL::BindFlags::CopyDst | LLGL::BindFlags::Sampled;
    texture_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
    texture_desc.debugName = "ImGuiFontTexture";

    font_texture_ = g_device->CreateTexture(texture_desc);

    LLGL::ImageView image_view;
    image_view.format = LLGL::ImageFormat::RGBA;
    image_view.dataType = LLGL::DataType::UInt8;
    image_view.data = pixels;
    image_view.dataSize = pixelsByteSize;

    LLGL::TextureRegion texture_region;
    texture_region.extent = { (uint32_t)width, (uint32_t)height, 1 };

    g_device->WriteTexture(*font_texture_, texture_region, image_view);

    uint64_t font_texture_idx = allocate_imgui_descriptor(font_texture_);

    io.Fonts->SetTexID(static_cast<ImTextureID>(font_texture_idx));
}

LLGL::Shader* ImGuiRenderer::load_shader(std::string_view shader_name, LLGL::ShaderType shader_type) {
#if defined(_WIN32)
    std::string shader_name_ext = std::format("{}.hlsl", shader_name);
#elif defined(__APPLE__)
    std::string shader_name_ext = std::format("{}.metal", shader_name);
#endif 

    std::string shader_content;
    std::string full_path = std::format("{}/shaders/{}", FileSystem::get_source_path(), shader_name_ext);

    FileSystem::read(full_path, shader_content);

    LLGL::ShaderDescriptor shader_desc;
    shader_desc.sourceType = LLGL::ShaderSourceType::CodeString;
    shader_desc.source = shader_content.c_str();
    shader_desc.type = shader_type;
    
#if defined(_WIN32)
    shader_desc.entryPoint = "main";

    switch (shader_type) {
        case LLGL::ShaderType::Compute:
            shader_desc.profile = "cs_5_1";
            break;
        case LLGL::ShaderType::Vertex:
            shader_desc.profile = "vs_5_1";
            break;
        case LLGL::ShaderType::Fragment:
            shader_desc.profile = "ps_5_1";
            break;
        default:
            LOGT_ERROR(LogRenderer, "Unsupported shader type!");
            return nullptr;
    }
#elif defined(__APPLE__)
    shader_desc.profile = "2.1";

    switch (shader_type) {
        case LLGL::ShaderType::Compute:
            shader_desc.entryPoint = "kernel_main";
            break;
        case LLGL::ShaderType::Vertex:
            shader_desc.entryPoint = "vertex_main";
            break;
        case LLGL::ShaderType::Fragment:
            shader_desc.entryPoint = "fragment_main";
            break;
        default:
            LOGT_ERROR(LogRenderer, "Unsupported shader type!");
            return nullptr;
    }
#endif

    if (shader_type == LLGL::ShaderType::Vertex) {
        shader_desc.vertex.inputAttribs = g_vertex_attribs;
    }

    LLGL::Shader* shader = g_device->CreateShader(shader_desc);

    if (!shader) {
        LOGT_ERROR(LogRenderer, "Failed to load shader %s", full_path.c_str());
    }

    if (shader && shader->GetReport()) {
        const LLGL::Report* report = shader->GetReport();

        if (report->HasErrors()) {
            LOGT_ERROR(LogRenderer, "%s", report->GetText());
        }
    }

    return shader;
}

LLGL::Buffer* ImGuiRenderer::get_vertex_buffer(uint32_t buffer_size) {
    LLGL::Buffer* buffer = vertex_buffers_[g_frame_index];

    if (buffer->GetDesc().size < buffer_size) {
        g_device->Release(*buffer);
        
        LLGL::BufferDescriptor vertex_buffer_desc;
        vertex_buffer_desc.size = buffer_size * 2;
        vertex_buffer_desc.bindFlags = LLGL::BindFlags::VertexBuffer;
        vertex_buffer_desc.vertexAttribs = g_vertex_attribs;
        vertex_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;

        vertex_buffers_[g_frame_index] = g_device->CreateBuffer(vertex_buffer_desc);
    }

    return vertex_buffers_[g_frame_index];
}

LLGL::Buffer* ImGuiRenderer::get_index_buffer(uint32_t buffer_size) {
    LLGL::Buffer* buffer = index_buffers_[g_frame_index];

    if (buffer->GetDesc().size < buffer_size) {
        g_device->Release(*buffer);
        
        LLGL::BufferDescriptor index_buffer_desc;
        index_buffer_desc.size = buffer_size * 2;
        index_buffer_desc.bindFlags = LLGL::BindFlags::IndexBuffer;
        index_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
        index_buffer_desc.format = index_format_;

        index_buffers_[g_frame_index] = g_device->CreateBuffer(index_buffer_desc);
    }

    return index_buffers_[g_frame_index];
}

}