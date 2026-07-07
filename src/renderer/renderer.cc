#include "renderer.h"
#include "device.h"
#include "viewport_grid_settings.h"
#include "log_categories.h"
#include "core/window_.h"
#include "core/file_system/file_system.h"
#include "editor/editor.h"
#include "application/camera.h"
#include "geom/mesh_component.h"
#include "math/entities/math_entity.h"
#include "math/pde/pde_component.h"

#include <imgui/imgui.h>
#include <LLGL/LLGL.h>
#include <LLGL/Utils/VertexFormat.h>
#include <stb/stb_image_write.h>

#include <cmath>
#include <vector>

#if defined(_WIN32)
    constexpr const char* g_backend_name = "Direct3D12";
#elif defined(__APPLE__)
    constexpr const char* g_backend_name = "Metal";
#endif 

namespace fem {

void LogCallbackLLGL(LLGL::Log::ReportType type, const char* text, void* userData, const LLGL::Log::ColorCodes& colors)
{
    switch (type)
    {
        case LLGL::Log::ReportType::Default:
        {
            LOGT_DEBUG(LogRenderer, text);
            break;
        }
        case LLGL::Log::ReportType::Error:
        {
            LOGT_ERROR(LogRenderer, text);
            break;
        }
    }
}

bool Renderer::init(const RendererInitInfo& init_info) {
    if (is_initialized_) return true;

    if (!init_info.window) {
        return false;
    }

    window_ = init_info.window;

    glm::uvec2 framebuffer_size = init_info.window->frame_buffer_size();

    if (!framebuffer_size.x || !framebuffer_size.y) {
        framebuffer_size.x = framebuffer_size.x > 0 ? framebuffer_size.x : 1280;
        framebuffer_size.y = framebuffer_size.y > 0 ? framebuffer_size.y : 1280;
    }

    LLGL::Log::RegisterCallback(LogCallbackLLGL);

    LLGL::RenderSystemDescriptor renderer_desc;
    renderer_desc.moduleName = g_backend_name;
    renderer_desc.debugger = &debugger_;

    LLGL::Report report;
    g_device = LLGL::RenderSystem::Load(renderer_desc, &report);

    if (!g_device) {
        LOGT_ERROR(LogRenderer, report.GetText());
        return false;
    }

    surface_ = std::make_shared<Surface>(window_);

    LLGL::SwapChainDescriptor swap_chain_desc;
    swap_chain_desc.resolution = { framebuffer_size.x, framebuffer_size.y };
    swap_chain_desc.depthBits = 0;
    swap_chain_desc.stencilBits = 0;
    
    swap_chain_ = g_device->CreateSwapChain(swap_chain_desc, surface_);
    swap_chain_->SetVsyncInterval(1);

    main_cmd_ = g_device->CreateCommandBuffer(LLGL::CommandBufferFlags::ImmediateSubmit);

    create_dummy_textures();
    create_samplers();

    shader_manager_ = std::make_unique<ShaderManager>();
    imgui_renderer_ = std::make_unique<ImGuiRenderer>();

    imgui_renderer_->init({
        .swap_chain = swap_chain_,
        .shader_manager = shader_manager_.get()
    });

    {
        LLGL::TextureDescriptor color_texture_desc;
        color_texture_desc.type = LLGL::TextureType::Texture2D;
        color_texture_desc.extent = {1920, 1080, 1};
        color_texture_desc.format = LLGL::Format::RGBA8UNorm;
        color_texture_desc.bindFlags = LLGL::BindFlags::ColorAttachment | LLGL::BindFlags::Sampled | LLGL::BindFlags::CopySrc;
        color_texture_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
        viewport_color_texture_ = g_device->CreateTexture(color_texture_desc);

        LLGL::TextureDescriptor depth_texture_desc = color_texture_desc;
        depth_texture_desc.samples = 8;
        depth_texture_desc.type = LLGL::TextureType::Texture2DMS;
        depth_texture_desc.format = LLGL::Format::D32Float;
        depth_texture_desc.bindFlags = LLGL::BindFlags::DepthStencilAttachment;
        depth_texture_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
        viewport_depth_texture_ = g_device->CreateTexture(depth_texture_desc);

        LLGL::TextureDescriptor color_readback_texture_desc = color_texture_desc;
        color_readback_texture_desc.bindFlags = LLGL::BindFlags::CopyDst;
        color_readback_texture_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Read;
        viewport_color_readback_texture_ = g_device->CreateTexture(color_readback_texture_desc);

        viewport_imgui_descriptor_ = imgui_renderer_->allocate_imgui_descriptor(viewport_color_texture_);

        LLGL::RenderTargetDescriptor render_target_desc;
        render_target_desc.colorAttachments[0].format = LLGL::Format::RGBA8UNorm;
        render_target_desc.resolution = { 1920, 1080 };
        render_target_desc.depthStencilAttachment = viewport_depth_texture_;
        render_target_desc.resolveAttachments[0].texture = viewport_color_texture_;
        render_target_desc.samples = 8;

        viewport_render_target_ = g_device->CreateRenderTarget(render_target_desc);
    }

    create_object_pipeline();
    create_grid_pipeline();

    is_initialized_ = true;

    return true;
}

void Renderer::shutdown() {
    auto release = [](auto*& resource) {
        if (resource) {
            g_device->Release(*resource);
            resource = nullptr;
        }
    };

    if (imgui_renderer_) {
        imgui_renderer_->shutdown();
    }

    release(swap_chain_);
    release(main_cmd_);
    release(viewport_color_texture_);
    release(viewport_depth_texture_);
    release(viewport_color_readback_texture_);
    release(viewport_render_target_);
    release(object_pipeline_);
    release(object_pipeline_layout_);
    release(object_resource_heap_);
    release(object_vs_constant_buffer_);
    release(object_ps_constant_buffer_);
    release(grid_vertex_buffer_);
    release(grid_vs_constant_buffer_);
    release(grid_ps_constant_buffer_);
    release(grid_resource_heap_);
    release(grid_pipeline_layout_);
    release(grid_pipeline_);

    shader_manager_.reset();

    is_initialized_ = false;
}


void Renderer::begin_frame() {

}

namespace {

struct GridDrawRanges {
    uint32_t minor_count = 0;
    uint32_t major_count = 0;
    uint32_t x_axis_start = 0;
    uint32_t x_axis_count = 0;
    uint32_t z_axis_start = 0;
    uint32_t z_axis_count = 0;
    uint32_t y_axis_start = 0;
    uint32_t y_axis_count = 0;
};

GridDrawRanges build_grid_vertices(std::vector<glm::vec3>& out_vertices, const glm::vec3& camera_pos, const ViewportGridSettings& settings);

} // namespace

void Renderer::draw(const RendererDrawInfo& draw_info) {
    constexpr float viewport_background_color[] = {0.1f, 0.1f, 0.1f, 1.0f};
    constexpr float viewport_transparent_background_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    constexpr float swapchain_background_color[] = {0.1f, 0.1f, 0.1f, 1.0f};
    
    if (!is_initialized_) return;

    const EditorDrawResult* editor_info = draw_info.editor_draw_result;
    const Viewport3DCaptureSettings* capture_settings = editor_info->viewport_capture_settings;

    main_cmd_->Begin();

    if (editor_info->is_viewport_active)
    {
        const bool use_transparent_background = capture_settings->need_capture && capture_settings->use_transparent_background;

        main_cmd_->PushDebugGroup("render_viewport");
        main_cmd_->BeginRenderPass(*viewport_render_target_);
        main_cmd_->Clear(
            LLGL::ClearFlags::ColorDepth, 
            { 
                use_transparent_background ? viewport_transparent_background_color : viewport_background_color, 
                1.0f 
            }
        );

        LLGL::Extent2D viewport_size = {
            viewport_color_texture_->GetDesc().extent.width,
            viewport_color_texture_->GetDesc().extent.height
        };

        main_cmd_->SetViewport(viewport_size);

        if (editor_info->selected_entity->is_a<MathEntity>() && draw_info.camera) {
            glm::mat4x4 proj = glm::perspective(glm::radians(65.0f), (float)viewport_size.width / viewport_size.height, 0.01f, 5000.0f);
            glm::mat4x4 viewProj = proj * draw_info.camera->getViewMatrix();

            const ViewportGridSettings& grid_settings = editor_info->viewport_grid_settings;
            if (grid_settings.enabled && grid_pipeline_ && grid_resource_heap_ && grid_vertex_buffer_ && grid_vs_constant_buffer_ && grid_ps_constant_buffer_) {
                main_cmd_->PushDebugGroup("draw_grid");

                std::vector<glm::vec3> grid_vertices;
                GridDrawRanges ranges = build_grid_vertices(grid_vertices, draw_info.camera->cameraPos, grid_settings);

                if (!grid_vertices.empty()) {
                    g_device->WriteBuffer(*grid_vertex_buffer_, 0, grid_vertices.data(), grid_vertices.size() * sizeof(glm::vec3));
                    g_device->WriteBuffer(*grid_vs_constant_buffer_, 0, &viewProj, sizeof(glm::mat4x4));

                    main_cmd_->SetPipelineState(*grid_pipeline_);
                    main_cmd_->SetVertexBuffer(*grid_vertex_buffer_);

                    uint32_t offset = 0;
                    if (ranges.minor_count > 0) {
                        const glm::vec4 color(grid_settings.color_rgb, std::clamp(grid_settings.minor_alpha * 0.6f, 0.0f, 1.0f));
                        g_device->WriteBuffer(*grid_ps_constant_buffer_, 0, &color, sizeof(glm::vec4));
                        main_cmd_->SetResourceHeap(*grid_resource_heap_);
                        main_cmd_->Draw(ranges.minor_count, offset);
                        offset += ranges.minor_count;
                    }
                    if (ranges.major_count > 0) {
                        const glm::vec4 color(grid_settings.color_rgb, std::clamp(grid_settings.major_alpha * 0.7f, 0.0f, 1.0f));
                        g_device->WriteBuffer(*grid_ps_constant_buffer_, 0, &color, sizeof(glm::vec4));
                        main_cmd_->SetResourceHeap(*grid_resource_heap_);
                        main_cmd_->Draw(ranges.major_count, offset);
                        offset += ranges.major_count;
                    }
                    if (ranges.x_axis_count > 0) {
                        const glm::vec4 color(1.0f, 0.0f, 0.0f, 0.75f);
                        g_device->WriteBuffer(*grid_ps_constant_buffer_, 0, &color, sizeof(glm::vec4));
                        main_cmd_->SetResourceHeap(*grid_resource_heap_);
                        main_cmd_->Draw(ranges.x_axis_count, ranges.x_axis_start);
                    }
                    if (ranges.z_axis_count > 0) {
                        const glm::vec4 color(0.0f, 0.3f, 1.0f, 0.75f);
                        g_device->WriteBuffer(*grid_ps_constant_buffer_, 0, &color, sizeof(glm::vec4));
                        main_cmd_->SetResourceHeap(*grid_resource_heap_);
                        main_cmd_->Draw(ranges.z_axis_count, ranges.z_axis_start);
                    }
                    if (ranges.y_axis_count > 0) {
                        const glm::vec4 color(0.9f, 0.9f, 0.9f, 0.55f);
                        g_device->WriteBuffer(*grid_ps_constant_buffer_, 0, &color, sizeof(glm::vec4));
                        main_cmd_->SetResourceHeap(*grid_resource_heap_);
                        main_cmd_->Draw(ranges.y_axis_count, ranges.y_axis_start);
                    }
                }

                main_cmd_->PopDebugGroup();
            }

            if (!object_pipeline_ || !object_resource_heap_ || !object_vs_constant_buffer_ || !object_ps_constant_buffer_) {
                static bool logged_missing_object_resources = false;
                if (!logged_missing_object_resources) {
                    LOGT_ERROR(LogRenderer, "Skipping object draw because GPU resources are incomplete");
                    logged_missing_object_resources = true;
                }
            } else {
                main_cmd_->PushDebugGroup("draw_object_meshes");
                MathEntity* entity = static_cast<MathEntity*>(editor_info->selected_entity);
                PDEComponent* pde = entity->pde_component();

                g_device->WriteBuffer(*object_vs_constant_buffer_, 0, &viewProj, sizeof(glm::mat4x4));

                main_cmd_->SetPipelineState(*object_pipeline_);
                main_cmd_->SetResourceHeap(*object_resource_heap_);

                for (MeshComponent* mesh : entity->mesh_components()) {
                    LLGL::Buffer* vertex_buffer = mesh->vertex_buffer();
                    LLGL::Buffer* index_buffer = mesh->index_buffer();

                    if (!vertex_buffer || !index_buffer) {
                        continue;
                    }

                    const DifferentialEquationSolution& solution = pde->solution(mesh);
                    glm::vec4 ps_params;

                    if (mesh->has_display_u_bounds()) {
                        auto [u_min, u_max] = mesh->display_u_bounds();
                        ps_params.x = u_min;
                        ps_params.y = u_max;
                    } else {
                        ps_params.x = (float)solution.u_min;
                        ps_params.y = (float)solution.u_max;
                    }

                    g_device->WriteBuffer(*object_ps_constant_buffer_, 0, &ps_params, sizeof(glm::vec4));

                    main_cmd_->SetVertexBuffer(*vertex_buffer);
                    main_cmd_->SetIndexBuffer(*index_buffer);
                    main_cmd_->DrawIndexed(mesh->index_count(), 0);
                }

                main_cmd_->PopDebugGroup();
            }
        }

        main_cmd_->EndRenderPass();
        main_cmd_->PopDebugGroup();
    }

    {
        main_cmd_->BeginRenderPass(*swap_chain_);
        main_cmd_->Clear(LLGL::ClearFlags::Color, { swapchain_background_color });
        main_cmd_->SetViewport(swap_chain_->GetResolution());
    
        main_cmd_->PushDebugGroup("render_ui");
    
        imgui_renderer_->draw({
            .draw_data = draw_info.imgui_draw_data,
            .cmd = main_cmd_
        });
    
        main_cmd_->PopDebugGroup();
        main_cmd_->EndRenderPass();
    }

    main_cmd_->End();

    g_device->GetCommandQueue()->Submit(*main_cmd_);
    
    if (capture_settings->need_capture) {
        g_device->GetCommandQueue()->WaitIdle();

        const uint32_t width  = viewport_color_texture_->GetDesc().extent.width;
        const uint32_t height = viewport_color_texture_->GetDesc().extent.height;
        const uint32_t channels = 4;
        const uint32_t stride = width * channels;
        const std::string& export_path = capture_settings->export_path;

        main_cmd_->Begin();
        LLGL::TextureLocation loc;
        main_cmd_->CopyTexture(
            *viewport_color_readback_texture_, loc,
            *viewport_color_texture_, loc,
            { width, height, 1 }
        );
        main_cmd_->End();
        g_device->GetCommandQueue()->Submit(*main_cmd_);
        g_device->GetCommandQueue()->WaitIdle();

        LLGL::MutableImageView dstImageView;
        dstImageView.format   = LLGL::ImageFormat::RGBA;
        dstImageView.dataType = LLGL::DataType::UInt8;
        std::vector<uint8_t> imageBuffer(width * height * channels);
        dstImageView.data     = imageBuffer.data();
        dstImageView.dataSize = imageBuffer.size();

        LLGL::TextureRegion texture_region;
        texture_region.extent = { width, height, 1 };

        g_device->ReadTexture(*viewport_color_readback_texture_, texture_region, dstImageView);

        stbi_write_png(export_path.c_str(), width, height, channels, imageBuffer.data(), stride);
    }

    swap_chain_->Present();

    ++g_frame_number;

    g_frame_index = g_frame_number % swap_chain_->GetNumSwapBuffers();
}

void Renderer::draw_debug_info() const {
    ImGui::Begin("Debug Info"); 
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    //ImGui::Text("Vertices: %zu", mesh_->get_vertices().size());
    //ImGui::Text("Indices: %zu", mesh_->get_indices().size());
    
    ImGui::End();
}

ImTextureID Renderer::get_viewport_texture_id() const {
    return viewport_imgui_descriptor_;
}

void Renderer::create_object_pipeline() {
    constexpr const char* shader_path = "object";

    object_program_ = shader_manager_->graphics_shader_program({{ shader_path }});

    if (!object_program_ || !object_program_->is_valid()) {
        LOGT_ERROR(LogRenderer, "Cannot create object pipeline without valid shaders");
        return;
    }

    LLGL::PipelineLayoutDescriptor layout_desc;
    layout_desc.heapBindings = {
#if defined(__APPLE__)
        { LLGL::ResourceType::Buffer, LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::VertexStage, 1 },
        { LLGL::ResourceType::Buffer, LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::FragmentStage, 2 },
#else
        { LLGL::ResourceType::Buffer, LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::VertexStage, 0 },
        { LLGL::ResourceType::Buffer, LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::FragmentStage, 1 },
#endif
    };

    object_pipeline_layout_ = g_device->CreatePipelineLayout(layout_desc);

    LLGL::GraphicsPipelineDescriptor pipeline_desc;
    pipeline_desc.vertexShader = object_program_->vertex_shader().handle();
    pipeline_desc.fragmentShader = object_program_->fragment_shader().handle();
    pipeline_desc.pipelineLayout = object_pipeline_layout_;
    pipeline_desc.rasterizer.multiSampleEnabled = false;
    pipeline_desc.primitiveTopology = LLGL::PrimitiveTopology::TriangleList;
    pipeline_desc.indexFormat = LLGL::Format::R32UInt;
    pipeline_desc.renderPass = viewport_render_target_->GetRenderPass();
    pipeline_desc.depth.testEnabled = true;
    pipeline_desc.depth.writeEnabled = true;
    pipeline_desc.rasterizer.multiSampleEnabled = true;

    object_pipeline_ = g_device->CreatePipelineState(pipeline_desc);

    if (object_pipeline_ && object_pipeline_->GetReport()) {
        const LLGL::Report* report = object_pipeline_->GetReport();

        if (report->HasErrors()) {
            LOGT_ERROR(LogRenderer, "%s", report->GetText());
            g_device->Release(*object_pipeline_);
            object_pipeline_ = nullptr;
            return;
        }
    }

    LLGL::BufferDescriptor vs_constant_buffer_desc;
    vs_constant_buffer_desc.size = sizeof(glm::mat4x4);
    vs_constant_buffer_desc.bindFlags = LLGL::BindFlags::ConstantBuffer;
    vs_constant_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
    
    object_vs_constant_buffer_ = g_device->CreateBuffer(vs_constant_buffer_desc);

    LLGL::BufferDescriptor ps_constant_buffer_desc;
    ps_constant_buffer_desc.size = sizeof(glm::vec4);
    ps_constant_buffer_desc.bindFlags = LLGL::BindFlags::ConstantBuffer;
    ps_constant_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;

    object_ps_constant_buffer_ = g_device->CreateBuffer(ps_constant_buffer_desc);

    LLGL::ResourceViewDescriptor resource_view_descs[] = {
        object_vs_constant_buffer_,
        object_ps_constant_buffer_
    };

    object_resource_heap_ = g_device->CreateResourceHeap(object_pipeline_layout_, resource_view_descs);
}

void Renderer::create_grid_pipeline() {
    constexpr const char* shader_path = "grid";

    grid_program_ = shader_manager_->graphics_shader_program({{ shader_path }});

    if (!grid_program_ || !grid_program_->is_valid()) {
        LOGT_ERROR(LogRenderer, "Cannot create grid pipeline without valid shaders");
        return;
    }

    LLGL::PipelineLayoutDescriptor layout_desc;
    layout_desc.heapBindings = {
#if defined(__APPLE__)
        { LLGL::ResourceType::Buffer, LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::VertexStage, 1 },
        { LLGL::ResourceType::Buffer, LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::FragmentStage, 2 },
#else
        { LLGL::ResourceType::Buffer, LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::VertexStage, 0 },
        { LLGL::ResourceType::Buffer, LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::FragmentStage, 1 },
#endif
    };
    grid_pipeline_layout_ = g_device->CreatePipelineLayout(layout_desc);

    LLGL::GraphicsPipelineDescriptor pipeline_desc;
    pipeline_desc.vertexShader = grid_program_->vertex_shader().handle();
    pipeline_desc.fragmentShader = grid_program_->fragment_shader().handle();
    pipeline_desc.pipelineLayout = grid_pipeline_layout_;
    pipeline_desc.primitiveTopology = LLGL::PrimitiveTopology::LineList;
    pipeline_desc.renderPass = viewport_render_target_->GetRenderPass();
    pipeline_desc.depth.testEnabled = true;
    pipeline_desc.depth.writeEnabled = false;
    pipeline_desc.rasterizer.cullMode = LLGL::CullMode::Disabled;
    pipeline_desc.rasterizer.multiSampleEnabled = true;

    pipeline_desc.blend.targets[0].blendEnabled = true;
    pipeline_desc.blend.targets[0].srcColor = LLGL::BlendOp::SrcAlpha;
    pipeline_desc.blend.targets[0].dstColor = LLGL::BlendOp::InvSrcAlpha;
    pipeline_desc.blend.targets[0].colorArithmetic = LLGL::BlendArithmetic::Add;
    pipeline_desc.blend.targets[0].srcAlpha = LLGL::BlendOp::One;
    pipeline_desc.blend.targets[0].dstAlpha = LLGL::BlendOp::InvSrcAlpha;
    pipeline_desc.blend.targets[0].alphaArithmetic = LLGL::BlendArithmetic::Add;
    pipeline_desc.blend.targets[0].colorMask = LLGL::ColorMaskFlags::All;

    grid_pipeline_ = g_device->CreatePipelineState(pipeline_desc);

    if (grid_pipeline_ && grid_pipeline_->GetReport()) {
        const LLGL::Report* report = grid_pipeline_->GetReport();
        if (report->HasErrors()) {
            LOGT_ERROR(LogRenderer, "%s", report->GetText());
            g_device->Release(*grid_pipeline_);
            grid_pipeline_ = nullptr;
            return;
        }
    }

    LLGL::BufferDescriptor vs_constant_buffer_desc;
    vs_constant_buffer_desc.size = sizeof(glm::mat4x4);
    vs_constant_buffer_desc.bindFlags = LLGL::BindFlags::ConstantBuffer;
    vs_constant_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
    grid_vs_constant_buffer_ = g_device->CreateBuffer(vs_constant_buffer_desc);

    LLGL::BufferDescriptor ps_constant_buffer_desc;
    ps_constant_buffer_desc.size = sizeof(glm::vec4);
    ps_constant_buffer_desc.bindFlags = LLGL::BindFlags::ConstantBuffer;
    ps_constant_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
    grid_ps_constant_buffer_ = g_device->CreateBuffer(ps_constant_buffer_desc);

    LLGL::ResourceViewDescriptor resource_view_descs[] = {
        grid_vs_constant_buffer_,
        grid_ps_constant_buffer_,
    };
    grid_resource_heap_ = g_device->CreateResourceHeap(grid_pipeline_layout_, resource_view_descs);

    LLGL::VertexFormat vertex_format;
    vertex_format.attributes = {
        LLGL::VertexAttribute{"position", LLGL::Format::RGB32Float, 0, 0, sizeof(glm::vec3), 0}
    };

    LLGL::BufferDescriptor vertex_buffer_desc;
    vertex_buffer_desc.size = 12u * 65536u;
    vertex_buffer_desc.bindFlags = LLGL::BindFlags::VertexBuffer;
    vertex_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
    vertex_buffer_desc.vertexAttribs = vertex_format.attributes;
    grid_vertex_buffer_ = g_device->CreateBuffer(vertex_buffer_desc);
}

void Renderer::create_dummy_textures() {
    LLGL::TextureDescriptor texture_desc;
    texture_desc.type   = LLGL::TextureType::Texture2D;
    texture_desc.format = LLGL::Format::R8UNorm;
    texture_desc.extent = { 1, 1, 1};
    
    g_dummy_2d_texture = g_device->CreateTexture(texture_desc);
}

void Renderer::create_samplers() {
    LLGL::SamplerDescriptor sampler_desc;
    sampler_desc.addressModeU = LLGL::SamplerAddressMode::Clamp;
    sampler_desc.addressModeV = LLGL::SamplerAddressMode::Clamp;
    sampler_desc.addressModeW = LLGL::SamplerAddressMode::Clamp;
    sampler_desc.minFilter = LLGL::SamplerFilter::Linear;
    sampler_desc.magFilter = LLGL::SamplerFilter::Linear;
    sampler_desc.mipMapFilter = LLGL::SamplerFilter::Linear;

    g_linear_clamp_sampler = g_device->CreateSampler(sampler_desc);
}

namespace {

static bool is_major_line(int i, int major_every) {
    const int m = major_every > 0 ? major_every : 1;
    const int r = ((i % m) + m) % m;
    return r == 0;
}

GridDrawRanges build_grid_vertices(std::vector<glm::vec3>& out_vertices, const glm::vec3& camera_pos, const ViewportGridSettings& settings) {
    out_vertices.clear();

    // Grid in XZ plane at y=0, centered around camera
    constexpr float y = 0.0f;
    const float step = std::max(0.001f, settings.cell_size);
    const float desired_radius = std::max(step, settings.render_distance);
    const int half_lines = (int)std::ceil(desired_radius / step);
    constexpr int major_every = 5;
    const float radius = step * (float)half_lines;
    const float axis_len = std::max(500.0f, radius);

    const float snapped_x = std::floor(camera_pos.x / step) * step;
    const float snapped_z = std::floor(camera_pos.z / step) * step;

    std::vector<glm::vec3> minor;
    std::vector<glm::vec3> major;
    minor.reserve((size_t)(half_lines * 8 + 64));
    major.reserve((size_t)(half_lines * 4 + 64));

    for (int i = -half_lines; i <= half_lines; ++i) {
        // if (i == 0) continue;
        const float offset = (float)i * step;
        const bool is_major = is_major_line(i, major_every);
        auto& out = is_major ? major : minor;
        out.emplace_back(snapped_x + offset, y, snapped_z - radius);
        out.emplace_back(snapped_x + offset, y, snapped_z + radius);
        out.emplace_back(snapped_x - radius, y, snapped_z + offset);
        out.emplace_back(snapped_x + radius, y, snapped_z + offset);
    }

    GridDrawRanges ranges;

    out_vertices.insert(out_vertices.end(), minor.begin(), minor.end());
    ranges.minor_count = (uint32_t)minor.size();

    out_vertices.insert(out_vertices.end(), major.begin(), major.end());
    ranges.major_count = (uint32_t)major.size();

    ranges.x_axis_start = (uint32_t)out_vertices.size();
    out_vertices.emplace_back(-axis_len, 0.0f, 0.0f);
    out_vertices.emplace_back(+axis_len, 0.0f, 0.0f);
    ranges.x_axis_count = 2;

    ranges.z_axis_start = (uint32_t)out_vertices.size();
    out_vertices.emplace_back(0.0f, 0.0f, -axis_len);
    out_vertices.emplace_back(0.0f, 0.0f, +axis_len);
    ranges.z_axis_count = 2;

    ranges.y_axis_start = (uint32_t)out_vertices.size();
    out_vertices.emplace_back(0.0f, 0.0f, 0.0f);
    out_vertices.emplace_back(0.0f, +axis_len * 0.5f, 0.0f);
    ranges.y_axis_count = 2;

    return ranges;
}

} // namespace

}
