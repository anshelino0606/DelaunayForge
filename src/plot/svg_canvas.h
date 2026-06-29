#pragma once

#include "svg_common.h"
#include <string>
#include <sstream>
#include <vector>
#include <string_view>

namespace fem {

struct Color8;

namespace plot::svg {

class Canvas {
public:
    Canvas(int32_t width, int32_t height);

    // nullptr fill or stroke = NONE
    void add_rect(int32_t x, int32_t y, int32_t w, int32_t h, const Color8* fill, const Color8* stroke = nullptr, float stroke_w = 0);
    void add_gradient_rect(int32_t x, int32_t y, int32_t w, int32_t h, const std::string& gradient, const Color8* stroke = nullptr, float stroke_w = 0);
    
    void start_defs();
    void end_defs();

    void add_clip_path(const std::string& id, int32_t x, int32_t y, int32_t w, int32_t h);

    void start_linear_gradient(const std::string& id, float x1, float y1, float x2, float y2);
    void add_gradient_stop(float offset_pct, const Color8& c);
    void end_linear_gradient();

    void start_group(const std::string& clip_path_id = "");
    void end_group();

    void add_polygon(const std::vector<std::pair<float, float>>& pts, const Color8& fill, float opacity = 1.0f);
    void add_line(float x1, float y1, float x2, float y2, const Color8& c, float width);
    void add_circle(float cx, float cy, float r, const Color8& c);
    void add_text(float x, float y, const std::string& text, const Color8& c, int32_t size, const std::string& anchor = std::string(svg::val::START));
    void add_text_5x7(float x, float y, std::string_view text, const Color8& color, int32_t px_scale, int32_t anchor);

    bool save_svg(const std::string& path);

private:
    int32_t width_ = 0; 
    int32_t height_ = 0;
    std::ostringstream ss_;

    std::string escape(std::string_view data);
    std::string to_rgb(const Color8& c);
    double opacity(const Color8& c);

    std::string make_attr(std::string_view name, std::string_view value) const;
    std::string make_attr(std::string_view name, double value) const;
};

}

}