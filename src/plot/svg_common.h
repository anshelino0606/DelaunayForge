#pragma once

#include <string_view>

namespace fem::plot::svg {

constexpr std::string_view XML_HEADER = R"(<?xml version="1.0" encoding="UTF-8"?>)";
constexpr std::string_view SVG_CLOSE  = "</svg>\n";

namespace attr {

constexpr std::string_view X               = "x";
constexpr std::string_view Y               = "y";
constexpr std::string_view CX              = "cx";
constexpr std::string_view CY              = "cy";
constexpr std::string_view R               = "r";
constexpr std::string_view WIDTH           = "width";
constexpr std::string_view HEIGHT          = "height";
constexpr std::string_view FILL            = "fill";
constexpr std::string_view FILL_OPACITY    = "fill-opacity";
constexpr std::string_view STROKE          = "stroke";
constexpr std::string_view STROKE_WIDTH    = "stroke-width";
constexpr std::string_view STROKE_LINECAP  = "stroke-linecap";
constexpr std::string_view CLIP_PATH       = "clip-path";
constexpr std::string_view ID              = "id";
constexpr std::string_view X1              = "x1";
constexpr std::string_view Y1              = "y1";
constexpr std::string_view X2              = "x2";
constexpr std::string_view Y2              = "y2";
constexpr std::string_view OFFSET          = "offset";
constexpr std::string_view STOP_COLOR      = "stop-color";
constexpr std::string_view STOP_OPACITY    = "stop-opacity";
constexpr std::string_view POINTS          = "points";
constexpr std::string_view FONT_SIZE       = "font-size";
constexpr std::string_view FONT_FAMILY     = "font-family";
constexpr std::string_view TEXT_ANCHOR     = "text-anchor";
constexpr std::string_view DOM_BASELINE    = "dominant-baseline";

}

namespace val {

constexpr std::string_view NONE            = "none";
constexpr std::string_view ROUND           = "round";
constexpr std::string_view START           = "start";
constexpr std::string_view MIDDLE          = "middle";
constexpr std::string_view DEFAULT_FONT    = "Helvetica, Arial, sans-serif";

};

}