#pragma once

#include <cstdint>

namespace fem::plot {

struct ExportSettings {
    enum class Theme : uint8_t { 
        Dark, 
        Light 
    };

    Theme theme = Theme::Light;
    int scale_factor = 2;           // export resolution = canvas_size * scale_factor
    bool include_axes = true;
    bool include_solution = true;
    bool include_mesh = true;
    bool include_points = true;
    bool include_colorbar = true;   // only applies when PDE solution bounds are available
    bool include_boundary_conditions = true;
    bool include_bc_legend = true;  // show legend for boundary conditions

    int format_index = 0; // 0 = PNG, 1 = SVG
};

}