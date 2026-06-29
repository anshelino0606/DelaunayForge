#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <string_view>

namespace fem {

struct Color8;

namespace plot {

class VectorCanvas {
public:
    VectorCanvas(int32_t width, int32_t height);

    void add_rect(int32_t x, int32_t y, int32_t w, int32_t h, const std::string& fill, const std::string& stroke = std::string(Val::NONE), float stroke_w = 0);
    
    void start_defs();
    void end_defs();

    void add_clip_path(const std::string& id, int32_t x, int32_t y, int32_t w, int32_t h);

    void start_linear_gradient(const std::string& id, float x1, float y1, float x2, float y2);
    void add_gradient_stop(float offset_pct, const Color8& c);
    void end_linear_gradient();

    void start_group(const std::string& clip_path_id = "");
    void end_group();

    void add_polygon(const std::vector<std::pair<float, float>>& pts, const std::string& fill, float opacity = 1.0f);
    void add_line(float x1, float y1, float x2, float y2, const Color8& c, float width);
    void add_circle(float cx, float cy, float r, const Color8& c);
    void add_text(float x, float y, const std::string& text, const Color8& c, int32_t size, const std::string& anchor = std::string(Val::START));
    void add_text_5x7(float x, float y, std::string_view text, const Color8& color, int32_t px_scale, int32_t anchor);

    bool save_svg(const std::string& path);

    std::string to_rgb(const Color8& c);

    struct Attr {
        static constexpr std::string_view X               = "x";
        static constexpr std::string_view Y               = "y";
        static constexpr std::string_view CX              = "cx";
        static constexpr std::string_view CY              = "cy";
        static constexpr std::string_view R               = "r";
        static constexpr std::string_view WIDTH           = "width";
        static constexpr std::string_view HEIGHT          = "height";
        static constexpr std::string_view FILL            = "fill";
        static constexpr std::string_view FILL_OPACITY    = "fill-opacity";
        static constexpr std::string_view STROKE          = "stroke";
        static constexpr std::string_view STROKE_WIDTH    = "stroke-width";
        static constexpr std::string_view STROKE_LINECAP  = "stroke-linecap";
        static constexpr std::string_view CLIP_PATH       = "clip-path";
        static constexpr std::string_view ID              = "id";
        static constexpr std::string_view X1              = "x1";
        static constexpr std::string_view Y1              = "y1";
        static constexpr std::string_view X2              = "x2";
        static constexpr std::string_view Y2              = "y2";
        static constexpr std::string_view OFFSET          = "offset";
        static constexpr std::string_view STOP_COLOR      = "stop-color";
        static constexpr std::string_view STOP_OPACITY    = "stop-opacity";
        static constexpr std::string_view POINTS          = "points";
        static constexpr std::string_view FONT_SIZE       = "font-size";
        static constexpr std::string_view FONT_FAMILY     = "font-family";
        static constexpr std::string_view TEXT_ANCHOR     = "text-anchor";
        static constexpr std::string_view DOM_BASELINE    = "dominant-baseline";
    };

    struct Val {
        static constexpr std::string_view NONE            = "none";
        static constexpr std::string_view ROUND           = "round";
        static constexpr std::string_view START           = "start";
        static constexpr std::string_view MIDDLE          = "middle";
        static constexpr std::string_view DEFAULT_FONT    = "Helvetica, Arial, sans-serif";
    };

private:
    int32_t width_ = 0; 
    int32_t height_ = 0;
    std::ostringstream ss_;

    static constexpr std::string_view XML_HEADER = R"(<?xml version="1.0" encoding="UTF-8"?>)";
    static constexpr std::string_view SVG_CLOSE  = "</svg>\n";

    std::string escape(std::string_view data);
    double opacity(const Color8& c);

    std::string make_attr(std::string_view name, std::string_view value) const;
    std::string make_attr(std::string_view name, double value) const;
};

}

}