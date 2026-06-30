#pragma once

#include "scene_data.h"
#include <string>

namespace fem::plot {

bool export_svg(const std::string& absolute_path, const SceneData& scene_data);

}