#ifndef FEM_VIEWPORT_GRID_SETTINGS_H
#define FEM_VIEWPORT_GRID_SETTINGS_H

#include <glm/glm.hpp>

namespace fem {

struct ViewportGridSettings {
    bool enabled = true;

    glm::vec3 color_rgb = glm::vec3(0.80f);

    float minor_alpha = 0.08f;
    float major_alpha = 0.14f;

    float cell_size = 10.0f;

    float render_distance = 1200.0f;
};

}

#endif // FEM_VIEWPORT_GRID_SETTINGS_H
