#pragma once

#include "core/color.h"
#include <imgui.h>
#include <glm/vec2.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace fem::plot {

std::string fmt_no_plus(double x);
double nice_num(double x, bool round_it);
std::vector<double> nice_ticks(double lo, double hi, int32_t target_ticks = 6);
std::string fmt_tick(double v);
bool glyph_5x7(char c, uint8_t rows[7]);
glm::vec2 measure_text_5x7(std::string_view text, int px_scale);
Color8 color_for_u(double u_min, double u_max, double u, bool have_bounds);

}