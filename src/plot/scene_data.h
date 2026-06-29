#pragma once

#include "export_settings.h"
#include <glm/vec2.hpp>

namespace fem {

class PlanarMeshComponent;
class PDEComponent;

namespace plot {

struct SceneData {
    PlanarMeshComponent* last_mesh = nullptr;
    PDEComponent* pde = nullptr;
    glm::vec2 viewport_size = { 0.0f, 0.0f };
    float point_radius = 1.0f;
    ExportSettings settings;
};

}

}