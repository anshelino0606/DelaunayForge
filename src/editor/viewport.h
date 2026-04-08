#ifndef VIEWPORT_H
#define VIEWPORT_H

#include <imgui.h>
#include <glm/glm.hpp>
#include <algorithm>

class Viewport {
public:
    ImVec2 screen_pos;
    ImVec2 screen_size;

    float zoom = 1.0f;
    glm::vec2 pan = {0.0f, 0.0f};

    float zoom_min = 0.1f;
    float zoom_max = 50.0f;


    ImVec2 to_screen(const glm::dvec2& world_pos) const {
        return ImVec2(
            screen_pos.x + pan.x + (float)(world_pos.x * zoom),
            screen_pos.y + pan.y + (float)(world_pos.y * zoom)
        );
    }

    glm::dvec2 to_world(const ImVec2& pixel_pos) const {
        return glm::dvec2(
            (pixel_pos.x - screen_pos.x - pan.x) / zoom,
            (pixel_pos.y - screen_pos.y - pan.y) / zoom
        );
    }

    double pixels_to_world(double px) const {
        const ImVec2 c(screen_pos.x + 0.5f * screen_size.x,
                    screen_pos.y + 0.5f * screen_size.y);
        const glm::dvec2 w0 = to_world(c);
        const glm::dvec2 w1 = to_world(ImVec2(c.x + (float)px, c.y));
        return glm::length(w1 - w0);
    }

    
    void handle_input() {
        ImGuiIO& io = ImGui::GetIO();
        if (!ImGui::IsItemHovered()) return;

        if (io.MouseWheel != 0.0f) {
            glm::dvec2 mouse_world_before = to_world(io.MousePos);
            
            float zoom_factor = (io.MouseWheel > 0) ? 1.1f : 0.9f;
            zoom = std::clamp(zoom * zoom_factor, zoom_min, zoom_max);

            ImVec2 new_screen_pos = to_screen(mouse_world_before);
            pan.x += io.MousePos.x - new_screen_pos.x;
            pan.y += io.MousePos.y - new_screen_pos.y;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            pan.x += io.MouseDelta.x;
            pan.y += io.MouseDelta.y;
        }
    }
};

#endif // VIEWPORT_H