#pragma once

#include "scene_data.h"
#include <string>

namespace fem::plot {

bool svg(const std::string& absolute_path, const SceneData& scene_data);

}