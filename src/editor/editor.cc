#include "editor.h"
#include "widgets.h"
#include "core/window_.h"
#include "core/entity/entity.h"
#include "math/pde/pde_component.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "examples/ui_auto_gen_example.h"
#include "test/equtaion_test_window.h"
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>

namespace fem {

namespace {

inline const DifferentialEquationSolution* try_get_solution_ptr(const PDEComponent* pde, MeshComponent* mesh) noexcept {
    if (!pde) return nullptr;
    const auto& sol = pde->solution(mesh);
    return sol.is_ready() ? &sol : nullptr;
}

inline const CRS* try_get_last_A_ptr(const PDEComponent* pde) noexcept {
    if (!pde) return nullptr;
    const FEMSystem* sys = pde->last_system();
    return sys ? &sys->A : nullptr;
}

inline const IReferenceProvider* try_get_ref_ptr(const PDEComponent* pde) noexcept {
    return pde ? pde->reference_provider() : nullptr;
}

} // namespace


bool Editor::init(const EditorInitInfo& init_info) {
    if (is_initialized_) return true;

    if (!init_info.window 
        || !init_info.draw_debug_info_callback
    ) {
        return false;
    }

    window_ = init_info.window;
    draw_debug_info_callback_ = init_info.draw_debug_info_callback;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(init_info.window->window(), true);

    is_initialized_ = true;
    return true;
}

void Editor::reset() {
    outliner_ = OutlinerWindow();
    canvas_ = CanvasWindow();
    details_.reset();
    mesh_info_ = MeshElementInfoWindow();
    fem_error_ = FEMErrorAnalysisWindow();
    planar_mesh_generator_window_ = PlanarMeshGeneratorWindow();
}

void Editor::shutdown() {
    is_initialized_ = false;
}

EditorDrawResult Editor::draw(const EditorDrawInfo& draw_info) {
    if (!is_initialized_) {
        return {};
    }

    assert(draw_info.entities);

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        static uint64_t s_editor_frame_counter = 0;
        const bool log_this_frame = ((s_editor_frame_counter++ % 120u) == 0u);
        if (log_this_frame) {
            const ImGuiIO& io = ImGui::GetIO();
        }
    }
    docking_window_.draw({});
    // delaunay_gui_->render();
    draw_debug_info_callback_();

    main_menu_toolbar_.draw({
        .request_canvas_export_popup = &request_canvas_export_popup_,
        .request_viewport_capture_popup = &request_viewport_capture_popup_,
        .is_viewport_active = viewport_.is_active(),
        .viewport_grid_settings = &viewport_grid_settings_,
    });

    outliner_.draw({
        .entities = draw_info.entities
    });

    Entity* entity = outliner_.last_selected_entity();
    PlanarMeshComponent* selected_mesh = static_cast<PlanarMeshComponent*>(outliner_.selected_mesh_component());
    PDEComponent* selected_pde = entity ? entity->get_component<PDEComponent>() : nullptr;

    if (request_canvas_export_popup_) {
        canvas_.request_export_popup();
        request_canvas_export_popup_ = false;
    }

    if (request_viewport_capture_popup_) {
        viewport_.request_capture_popup();
        request_viewport_capture_popup_ = false;
    }

    canvas_.draw({
        .selected_entity = entity,
        .selected_mesh = selected_mesh,
        .selected_pde = selected_pde,
        .mesh_generator_state = &planar_mesh_generator_window_.state(),
        .triangulation_session_config = &planar_mesh_generator_window_.triangulation_session_config(),
        .on_inspector_pick = [&](PlanarMeshComponent* mesh,
                                const CanvasInspector::Selection& sel,
                                const glm::dvec2& wpos)
        {
            pick_.mesh = mesh;
            pick_.sel = sel;
            pick_.world_pos = wpos;
        }
    });

    viewport_.draw({
        .viewport_texture_id = draw_info.viewport_texture_id
    });

    details_.draw({
        .selected_entity = entity,
        .selected_mesh = selected_mesh,
        .canvas_state = &canvas_.state()
    });

    mesh_info_.draw({
        .mesh = pick_.mesh,
        .pde  = selected_pde,
        .sel  = pick_.sel,
    });

    fem_error_.draw({
        .mesh = pick_.mesh,
        .pde  = selected_pde,
        .sol  = try_get_solution_ptr(selected_pde, pick_.mesh),
        .A    = try_get_last_A_ptr(selected_pde),
        .ref  = try_get_ref_ptr(selected_pde),
        .sel  = pick_.sel,
    });



    planar_mesh_generator_window_.draw({
        .selected_mesh = selected_mesh,
        .canvas_state = &canvas_.state()
    });

    ImGui::Render();

    Widgets::execute_post_draw_callbacks();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 win_pos_pt = vp->Pos;

    glm::vec2 framebuffer_size = window_->frame_buffer_size();
    uint32_t width = framebuffer_size.x;
    uint32_t height = framebuffer_size.y;

    ImVec2 display_framebuffer_scale = ImGui::GetIO().DisplayFramebufferScale;
    uint32_t dfb_width = display_framebuffer_scale.x;
    uint32_t dfb_height = display_framebuffer_scale.y;

    EditorDrawResult draw_result;
    draw_result.selected_entity = entity;
    draw_result.viewport_size = viewport_.size();
    draw_result.viewport_pos = viewport_.position();
    draw_result.is_viewport_active = viewport_.is_active();
    draw_result.viewport_grid_settings = viewport_grid_settings_;
    draw_result.viewport_capture_settings = viewport_.capture_settings();

    return draw_result;
}

}