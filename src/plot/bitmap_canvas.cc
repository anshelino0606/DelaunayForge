#include "bitmap_canvas.h"
#include "utils.h"
#include "core/color.h"
#include <stb/stb_image_write.h>

namespace fem::plot {

BitmapCanvas::BitmapCanvas(int32_t width, int32_t height, Color8 clear_color)
    : width_(width), height_(height), pixels_(width * height, clear_color.rgba) {}

void BitmapCanvas::set_pixel(int32_t x, int32_t y, Color8 color) noexcept {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        pixels_[y * width_ + x] = color.rgba;
    }
}

void BitmapCanvas::blend_pixel(int32_t x, int32_t y, Color8 src) noexcept {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    
    uint32_t& dst_raw = pixels_[y * width_ + x];
    uint8_t sa = src.get_a();
    
    if (sa == 255) {
        dst_raw = src.rgba;
    } else if (sa > 0) {
        Color8 dst(dst_raw);
        float alpha = src.get_na();
        
        uint8_t r = static_cast<uint8_t>(src.get_r() * alpha + dst.get_r() * (1.0f - alpha));
        uint8_t g = static_cast<uint8_t>(src.get_g() * alpha + dst.get_g() * (1.0f - alpha));
        uint8_t b = static_cast<uint8_t>(src.get_b() * alpha + dst.get_b() * (1.0f - alpha));
        uint8_t a = std::max(sa, dst.get_a());
        
        dst_raw = Color8(r, g, b, a).rgba;
    }
}

void BitmapCanvas::draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, Color8 color, int32_t thickness) noexcept {
    int32_t dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int32_t sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx - dy, x = x0, y = y0;

    while (true) {
        for (int32_t ty = -thickness / 2; ty <= thickness / 2; ++ty) {
            for (int32_t tx = -thickness / 2; tx <= thickness / 2; ++tx) {
                blend_pixel(x + tx, y + ty, color);
            }
        }
        if (x == x1 && y == y1) break;
        int32_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}

void BitmapCanvas::fill_triangle(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, Color8 color) noexcept {
    int32_t x0 = static_cast<int32_t>(std::lround(a.x)), y0 = static_cast<int32_t>(std::lround(a.y));
    int32_t x1 = static_cast<int32_t>(std::lround(b.x)), y1 = static_cast<int32_t>(std::lround(b.y));
    int32_t x2 = static_cast<int32_t>(std::lround(c.x)), y2 = static_cast<int32_t>(std::lround(c.y));

    int32_t min_x = std::max(0, std::min({x0, x1, x2})), max_x = std::min(width_ - 1, std::max({x0, x1, x2}));
    int32_t min_y = std::max(0, std::min({y0, y1, y2})), max_y = std::min(height_ - 1, std::max({y0, y1, y2}));

    auto cross_product = [](int32_t px, int32_t py, int32_t ax, int32_t ay, int32_t bx, int32_t by) {
        return static_cast<float>((px - bx) * (ay - by) - (ax - bx) * (py - by));
    };

    for (int32_t y = min_y; y <= max_y; ++y) {
        for (int32_t x = min_x; x <= max_x; ++x) {
            float d1 = cross_product(x, y, x0, y0, x1, y1);
            float d2 = cross_product(x, y, x1, y1, x2, y2);
            float d3 = cross_product(x, y, x2, y2, x0, y0);
            if (!((d1 < 0.0f || d2 < 0.0f || d3 < 0.0f) && (d1 > 0.0f || d2 > 0.0f || d3 > 0.0f))) {
                blend_pixel(x, y, color);
            }
        }
    }
}

void BitmapCanvas::draw_circle(int32_t cx, int32_t cy, int32_t radius, Color8 color) noexcept {
    for (int32_t dy = -radius; dy <= radius; ++dy) {
        for (int32_t dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                set_pixel(cx + dx, cy + dy, color);
            }
        }
    }
}

void BitmapCanvas::draw_text_5x7(int32_t x, int32_t y, std::string_view text, Color8 color, int32_t px_scale, int32_t anchor) noexcept {
    glm::vec2 sz = measure_text_5x7(text, px_scale);
    int32_t xx = x - (anchor == 1 ? static_cast<int32_t>(std::lround(sz.x * 0.5f)) : (anchor == 2 ? static_cast<int32_t>(std::lround(sz.x)) : 0));

    const int32_t cw = 5 * px_scale;
    const int32_t sp = 1 * px_scale;
    int32_t pen_x = xx;
    uint8_t rows[7];

    for (char c : text) {
        if (!glyph_5x7(c, rows)) {
            pen_x += cw + sp;
            continue;
        }
        for (int32_t ry = 0; ry < 7; ++ry) {
            for (int32_t rx = 0; rx < 5; ++rx) {
                if (rows[ry] & (1u << (4 - rx))) {
                    for (int32_t sy = 0; sy < px_scale; ++sy) {
                        for (int32_t sx = 0; sx < px_scale; ++sx) {
                            blend_pixel(pen_x + rx * px_scale + sx, y + ry * px_scale + sy, color);
                        }
                    }
                }
            }
        }
        pen_x += cw + sp;
    }
}

bool BitmapCanvas::save_png(const std::string& path) const {
    return stbi_write_png(path.c_str(), width_, height_, 4, pixels_.data(), width_ * 4) != 0;
}

}