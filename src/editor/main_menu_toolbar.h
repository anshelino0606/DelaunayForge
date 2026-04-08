#ifndef FEM_MAIN_MENU_TOOLBAR_H
#define FEM_MAIN_MENU_TOOLBAR_H

#include <string>
#include <functional>

namespace fem {

struct ViewportGridSettings;

struct MainMenuToolbarDrawInfo {
    bool* request_canvas_export_popup = nullptr;
    bool* request_viewport_capture_popup = nullptr;

    bool is_viewport_active = false;

    ViewportGridSettings* viewport_grid_settings = nullptr;

};

enum class Theme { Dark, Light, Classic };

class MainMenuToolbar {
public:
    void draw(const MainMenuToolbarDrawInfo& draw_info);

private:
    bool show_grid_settings_window_ = false;
    Theme current_theme_ = Theme::Dark;

    struct FileMenuState {
        bool project_creation_requested = false;
        bool project_opening_requested = false;
        bool project_saving_requested = false;
        bool project_saving_as_requested = false;
        bool draw_save_project_popup = false;
        bool draw_project_name_popup = false;
        bool send_project_creation_request = false;

        std::string project_name;
        std::string chosen_directory;

        std::function<void()> project_creation_request;

        void invalidate() {
            *this = FileMenuState();
        }
    } file_menu_state_;

    void draw_toolbar(const MainMenuToolbarDrawInfo& draw_info);
    void draw_grid_settings_window(const MainMenuToolbarDrawInfo& draw_info);
    void draw_save_project_popup(const MainMenuToolbarDrawInfo& draw_info);
    void draw_project_name_popup(const MainMenuToolbarDrawInfo& draw_info);
    void handle_hotkeys(const MainMenuToolbarDrawInfo& draw_info);
    void process_file_menu_states(const MainMenuToolbarDrawInfo& draw_info);

    void start_creating_project();
    void start_opening_project();
    void start_saving_project();
    void start_saving_project_as();

    void create_project();
    void open_project();
    void save_project();
    void save_project_as();

    void open_save_project_popup();
};

}

#endif // FEM_MAIN_MENU_TOOLBAR_H