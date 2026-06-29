#pragma once

#include "core/color.h"

namespace fem::plot {

const Color8 COLOR_DARK_BG   (30,  30,  30);
const Color8 COLOR_DARK_FG   (220, 220, 220);
const Color8 COLOR_LIGHT_BG  (255, 255, 255);
const Color8 COLOR_LIGHT_FG  (30,  30,  30);

const Color8 MESH_DARK_VISIBLE_EDGE (200, 200, 200, 160);
const Color8 MESH_DARK_HIDDEN_EDGE  (100, 100, 100, 200);
const Color8 MESH_LIGHT_VISIBLE_EDGE(80,  80,  80,  120);
const Color8 MESH_LIGHT_HIDDEN_EDGE (50,  50,  50);
const Color8 MESH_DARK_BOUNDARY     (140, 200, 140, 240);
const Color8 MESH_LIGHT_BOUNDARY    (30,  30,  30);

const Color8 NODE_DARK_INTERNAL     (230, 120, 120, 230);
const Color8 NODE_DARK_BOUNDARY     (100, 200, 120, 230);
const Color8 NODE_LIGHT_INTERNAL    (180,  50,  50,  240);
const Color8 NODE_LIGHT_BOUNDARY    (40,  120,  50,  240);

const Color8 BC_DARK[4] = {
    {120, 120, 120}, // Default
    {255, 120, 200}, // Dirichlet
    {80,  220, 255}, // Neumann
    {255, 210, 80},  // Robin
};

const Color8 BC_LIGHT[4] = {
    {100, 100, 100}, // Default
    {180, 0,   140}, // Dirichlet
    {0,   130, 180}, // Neumann
    {200, 130, 0},   // Robin
};

constexpr double PADDING_SCALE_RATIO = 0.03;
constexpr float  POINT_RADIUS_SCALE  = 0.85f;
constexpr float  TRI_AVERAGE_DIVISOR = 3.0f;
constexpr float  FRAME_THICKNESS_MOD = 1.5f;
constexpr float  BC_THICKNESS_MOD    = 2.5f;
constexpr int32_t TICK_COUNT_TARGET  = 6;

}