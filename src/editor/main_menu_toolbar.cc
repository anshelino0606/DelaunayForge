#include "main_menu_toolbar.h"
#include "events.h"
#include "core/file_system/file_system.h"
#include "core/project_file_extensions.h"
#include "log_categories.h"
#include "renderer/viewport_grid_settings.h"
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace fem {

constexpr const char* g_save_project_popup_name = "Save Project?";
constexpr const char* g_project_name_popup_name = "Enter Project Name";

void MainMenuToolbar::draw(const MainMenuToolbarDrawInfo& draw_info) {
    draw_toolbar(draw_info);
    draw_grid_settings_window(draw_info);
    draw_save_project_popup(draw_info);
    draw_project_name_popup(draw_info);
    handle_hotkeys(draw_info);
    process_file_menu_states(draw_info);
}

void MainMenuToolbar::draw_toolbar(const MainMenuToolbarDrawInfo& draw_info) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project", "Ctrl+N")) {
                start_creating_project();
            }
            if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
                start_opening_project();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                start_saving_project();
            }
            if (ImGui::MenuItem("Save As", "Ctrl+Shift+S")) {
                start_saving_project_as();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Export Canvas...")) {
                if (draw_info.request_canvas_export_popup) {
                    *draw_info.request_canvas_export_popup = true;
                }
            }
            
            if (!draw_info.is_viewport_active) {
                ImGui::BeginDisabled();
            }

            if (ImGui::MenuItem("Capture 3D Viewport...")) {
                if (draw_info.request_viewport_capture_popup) {
                    *draw_info.request_viewport_capture_popup = true;
                }
            }

            if (!draw_info.is_viewport_active) {
                ImGui::EndDisabled();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            ViewportGridSettings* grid = draw_info.viewport_grid_settings;

            if (!grid) {
                ImGui::TextUnformatted("(No settings target)");
            } else {
                ImGui::Checkbox("Show Grid", &grid->enabled);

                if (ImGui::MenuItem("Grid Settings...")) {
                    show_grid_settings_window_ = true;
                }

                ImGui::SliderFloat("Cell Size", &grid->cell_size, 0.1f, 200.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Render Distance", &grid->render_distance, 10.0f, 10000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);

                if (grid->cell_size < 0.001f) grid->cell_size = 0.001f;
                if (grid->render_distance < grid->cell_size) grid->render_distance = grid->cell_size;

                if (ImGui::Button("Reset Grid Defaults")) {
                    *grid = ViewportGridSettings{};
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::TextUnformatted("Theme");
            ImGui::Separator();
            if (ImGui::RadioButton("Dark", current_theme_ == Theme::Dark)) {
                current_theme_ = Theme::Dark;
                ImGui::StyleColorsDark();
            }
            if (ImGui::RadioButton("Light", current_theme_ == Theme::Light)) {
                current_theme_ = Theme::Light;
                ImGui::StyleColorsLight();
            }
            if (ImGui::RadioButton("Classic", current_theme_ == Theme::Classic)) {
                current_theme_ = Theme::Classic;
                ImGui::StyleColorsClassic();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void MainMenuToolbar::draw_grid_settings_window(const MainMenuToolbarDrawInfo& draw_info) {
    if (!show_grid_settings_window_) return;

    ViewportGridSettings* grid = draw_info.viewport_grid_settings;
    if (!grid) return;

    if (ImGui::Begin("Grid Settings", &show_grid_settings_window_)) {
        ImGui::Checkbox("Enabled", &grid->enabled);

        float rgb[3] = { grid->color_rgb.r, grid->color_rgb.g, grid->color_rgb.b };
        if (ImGui::ColorPicker3("Grid Color", rgb, ImGuiColorEditFlags_Float)) {
            grid->color_rgb = glm::vec3(rgb[0], rgb[1], rgb[2]);
        }

        ImGui::SliderFloat("Minor Alpha", &grid->minor_alpha, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Major Alpha", &grid->major_alpha, 0.0f, 1.0f, "%.3f");

        ImGui::Separator();

        ImGui::SliderFloat("Cell Size", &grid->cell_size, 0.1f, 200.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Render Distance", &grid->render_distance, 10.0f, 10000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);

        if (grid->cell_size < 0.001f) grid->cell_size = 0.001f;
        if (grid->render_distance < grid->cell_size) grid->render_distance = grid->cell_size;

        if (ImGui::Button("Reset Defaults")) {
            *grid = ViewportGridSettings{};
        }
    }
    ImGui::End();
}

void MainMenuToolbar::draw_save_project_popup(const MainMenuToolbarDrawInfo& draw_info) {
    if (file_menu_state_.draw_save_project_popup) {
        ImGui::OpenPopup(g_save_project_popup_name);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(g_save_project_popup_name, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes.\nDo you want to save before continuing?\n\n");
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) { 
            file_menu_state_.project_saving_requested = true;
            file_menu_state_.draw_save_project_popup = false;
            ImGui::CloseCurrentPopup(); 
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
            file_menu_state_.draw_save_project_popup = false;
            ImGui::CloseCurrentPopup(); 
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            file_menu_state_.invalidate();
            ImGui::CloseCurrentPopup(); 
        }

        ImGui::EndPopup();
    }

}

void MainMenuToolbar::draw_project_name_popup(const MainMenuToolbarDrawInfo& draw_info) {
    if (file_menu_state_.draw_project_name_popup) {
        ImGui::OpenPopup(g_project_name_popup_name);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(g_project_name_popup_name, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Project Name", &file_menu_state_.project_name);
    
        if (file_menu_state_.project_name.empty()) {
            ImGui::BeginDisabled();
        }
    
        if (ImGui::Button("Continue")) {
            file_menu_state_.send_project_creation_request = true;
            file_menu_state_.draw_project_name_popup = false;
            ImGui::CloseCurrentPopup();
        }
    
        if (file_menu_state_.project_name.empty()) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
    
        if (ImGui::Button("Cancel")) {
            file_menu_state_.draw_project_name_popup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void MainMenuToolbar::handle_hotkeys(const MainMenuToolbarDrawInfo& draw_info) {
    ImGuiIO io = ImGui::GetIO();
    
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        start_saving_project();
    }
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S)) {
        start_saving_project_as();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        start_opening_project();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
        start_creating_project();
    }
}

void MainMenuToolbar::process_file_menu_states(const MainMenuToolbarDrawInfo& draw_info) {
    if (file_menu_state_.draw_save_project_popup) {
        return;
    }

    if (file_menu_state_.project_saving_requested) {
        save_project();
    }
    if (file_menu_state_.project_saving_as_requested) {
        save_project_as();
    }
    if (file_menu_state_.project_opening_requested) {
        open_project();
    }
    if (file_menu_state_.project_creation_requested) {
        create_project();
    }

    if (!file_menu_state_.draw_project_name_popup) {
        if (file_menu_state_.send_project_creation_request) {
            file_menu_state_.project_creation_request();
        }

        file_menu_state_.invalidate();
    }
}

void MainMenuToolbar::start_creating_project() {
    open_save_project_popup();
    file_menu_state_.project_creation_requested = true;
}

void MainMenuToolbar::start_opening_project() {
    open_save_project_popup();
    file_menu_state_.project_opening_requested = true;
}

void MainMenuToolbar::start_saving_project() {
    file_menu_state_.project_saving_requested = true;
}

void MainMenuToolbar::start_saving_project_as() {
    file_menu_state_.project_saving_as_requested = true;
}

void MainMenuToolbar::create_project() {
    file_menu_state_.chosen_directory = FileSystem::open_directory_dialog("Create Project");

    file_menu_state_.project_creation_request = [&]() {
        ProjectCreationRequest request(file_menu_state_.chosen_directory, file_menu_state_.project_name);
        request.enqueue();
    };

    file_menu_state_.draw_project_name_popup = !file_menu_state_.chosen_directory.empty();
    file_menu_state_.project_creation_requested = false;
}

void MainMenuToolbar::open_project() {
    std::string file_path = FileSystem::open_file_dialog("Open Project", {g_project_file_extension});
    ProjectOpenRequest request(file_path);
    request.enqueue();
}

void MainMenuToolbar::save_project() {
    if (FileSystem::get_project_path().empty()) {
        save_project_as();
    } else {
        ProjectSaveRequest request;
        request.enqueue();
    }

    file_menu_state_.project_saving_requested = false;
}

void MainMenuToolbar::save_project_as() {
    file_menu_state_.chosen_directory = FileSystem::open_directory_dialog("Save Project As");

    file_menu_state_.project_creation_request = [&]() {
        ProjectSaveAsRequest request(file_menu_state_.chosen_directory, file_menu_state_.project_name);
        request.enqueue();
    };

    file_menu_state_.draw_project_name_popup = !file_menu_state_.chosen_directory.empty();
    file_menu_state_.project_saving_as_requested = false;
}

void MainMenuToolbar::open_save_project_popup() {
    file_menu_state_.draw_save_project_popup = true;
}

}