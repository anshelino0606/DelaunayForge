#ifndef FEM_VIEWPORT_3D_H
#define FEM_VIEWPORT_3D_H

#include <imgui/imgui.h>
#include <glm/glm.hpp>
#include <string>

namespace fem {

struct Viewport3DWindowDrawInfo {
    ImTextureID viewport_texture_id = 0;
};

struct Viewport3DCaptureSettings {
    bool need_capture = false;
    bool use_transparent_background = false;
    std::string export_path = s_default_export_path;

    constexpr static const char* s_default_export_path = "viewport.png";
};

class Viewport3DWindow {
public:
    void draw(const Viewport3DWindowDrawInfo& draw_info);

    void request_capture_popup() {
        request_capture_popup_ = true;
    }

    glm::uvec2 size() const { return size_; }
    glm::uvec2 position() const { return position_; }
    bool is_active() const { return is_active_; }

    Viewport3DCaptureSettings* capture_settings() {
        return &capture_settings_;
    }

private:
    glm::uvec2 size_;
    glm::uvec2 position_;

    bool is_active_ = false;
    bool request_capture_popup_ = false;
    bool open_capture_popup_ = false;

    Viewport3DCaptureSettings capture_settings_;

    void draw_capture_popup();
};

}

#endif // FEM_VIEWPORT_3D_H