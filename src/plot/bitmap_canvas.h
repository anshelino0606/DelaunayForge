#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <string_view>
#include <glm/glm.hpp>

namespace fem {

struct Color8;

namespace plot {

class BitmapCanvas {
public:
    BitmapCanvas(int32_t width, int32_t height, Color8 clear_color);
    ~BitmapCanvas() = default;

    int32_t width()  const noexcept { return width_; }
    int32_t height() const noexcept { return height_; }
    const uint32_t* data() const noexcept { return pixels_.data(); }

    void set_pixel(int32_t x, int32_t y, Color8 color) noexcept;
    void blend_pixel(int32_t x, int32_t y, Color8 src) noexcept;
    
    void draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, Color8 color, int32_t thickness = 1) noexcept;
    void fill_triangle(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, Color8 color) noexcept;
    void draw_circle(int32_t cx, int32_t cy, int32_t radius, Color8 color) noexcept;
    void draw_text_5x7(int32_t x, int32_t y, std::string_view text, Color8 color, int32_t px_scale, int32_t anchor) noexcept;

    bool save_png(const std::string& path) const;

private:
    int32_t width_;
    int32_t height_;
    std::vector<uint32_t> pixels_;
};

}

}