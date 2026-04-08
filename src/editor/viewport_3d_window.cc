#include "viewport_3d_window.h"
#include "core/file_system/file_system.h"
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace fem {

void Viewport3DWindow::draw(const Viewport3DWindowDrawInfo& draw_info) {
    is_active_ = ImGui::Begin("Viewport");

    if (!is_active_) {
        ImGui::End();
        return;
    }

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 extent = ImGui::GetContentRegionAvail();

    position_ = { pos.x, pos.y };
    size_ = { extent.x, extent.y };

    ImGui::Image(
        draw_info.viewport_texture_id,
        extent,
        ImVec2(0, 0),
        ImVec2(extent.x / 1920.0f, extent.y / 1080.0f)
    );

    draw_capture_popup();

    ImGui::End();
}

void Viewport3DWindow::draw_capture_popup() {
    constexpr const char* popup_name = "Capture 3D Viewport";
    constexpr ImVec2 default_button_size = ImVec2(120, 0);

    if (request_capture_popup_) {
        ImGui::OpenPopup(popup_name);
        request_capture_popup_ = false;
        open_capture_popup_ = true;
    }

    if (!open_capture_popup_) {
        return;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal(popup_name, &open_capture_popup_, ImGuiWindowFlags_AlwaysAutoResize)) {
        capture_settings_.export_path = Viewport3DCaptureSettings::s_default_export_path;
        capture_settings_.need_capture = false;
        capture_settings_.use_transparent_background = false;
        return;
    }

    ImGui::Checkbox("Use transparent background", &capture_settings_.use_transparent_background);

    ImGui::InputText("Path", &capture_settings_.export_path);
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        std::string path = FileSystem::save_file_dialog("Capture Viewport", { "png" });
        if (!path.empty()) {
            capture_settings_.export_path = path + ".png";
        }
    }

    capture_settings_.need_capture = ImGui::Button("Export", default_button_size);
    
    ImGui::SameLine();
    
    if (ImGui::Button("Close", default_button_size)) {
        open_capture_popup_ = false;
        capture_settings_.need_capture = false;
        capture_settings_.use_transparent_background = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

}