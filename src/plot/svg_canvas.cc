#include "svg_canvas.h"
#include "utils.h"
#include "core/color.h"
#include "core/file_system/file_system.h"
#include <format>

namespace fem::plot::svg {

Canvas::Canvas(int32_t width, int32_t height) : width_(width), height_(height) {
    ss_ << XML_HEADER << "\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\""
        << make_attr(attr::WIDTH, width_) 
        << make_attr(attr::HEIGHT, height_)
        << " viewBox=\"0 0 " << width_ << " " << height_ << "\">\n";
}

void Canvas::add_rect(int32_t x, int32_t y, int32_t w, int32_t h, const Color8* fill, const Color8* stroke, float stroke_w) {
    std::string fill_str   = fill   ? to_rgb(*fill)   : std::string(val::NONE);
    std::string stroke_str = stroke ? to_rgb(*stroke) : std::string(val::NONE);

    ss_ << tag::RECT_OPEN 
        << make_attr(attr::X, x) << make_attr(attr::Y, y) 
        << make_attr(attr::WIDTH, w) << make_attr(attr::HEIGHT, h)
        << make_attr(attr::FILL, fill_str) << make_attr(attr::STROKE, stroke_str)
        << make_attr(attr::STROKE_WIDTH, stroke_w) << tag::RECT_CLOSE;
}

void Canvas::add_gradient_rect(int32_t x, int32_t y, int32_t w, int32_t h, const std::string& gradient, const Color8* stroke, float stroke_w) {
    std::string stroke_str = stroke ? to_rgb(*stroke) : std::string(val::NONE);

    ss_ << tag::RECT_OPEN 
        << make_attr(attr::X, x) << make_attr(attr::Y, y) 
        << make_attr(attr::WIDTH, w) << make_attr(attr::HEIGHT, h)
        << make_attr(attr::FILL, gradient) << make_attr(attr::STROKE, stroke_str)
        << make_attr(attr::STROKE_WIDTH, stroke_w) << tag::RECT_CLOSE;
}

void Canvas::start_defs() { 
    ss_ << tag::DEFS_OPEN; 
}

void Canvas::end_defs() { 
    ss_ << tag::DEFS_CLOSE; 
}

void Canvas::add_clip_path(const std::string& id, int32_t x, int32_t y, int32_t w, int32_t h) {
    ss_ << tag::CLIP_PATH_OPEN << make_attr(attr::ID, id) << ">" << tag::RECT_OPEN
        << make_attr(attr::X, x) << make_attr(attr::Y, y) 
        << make_attr(attr::WIDTH, w) << make_attr(attr::HEIGHT, h) 
        << tag::RECT_CLOSE << tag::CLIP_PATH_CLOSE;
}

void Canvas::start_linear_gradient(const std::string& id, float x1, float y1, float x2, float y2) {
    ss_ << tag::LINEAR_GRAD_OPEN << make_attr(attr::ID, id)
        << make_attr(attr::X1, x1) << make_attr(attr::Y1, y1) 
        << make_attr(attr::X2, x2) << make_attr(attr::Y2, y2) << tag::END_NEW_LINE;
}

void Canvas::add_gradient_stop(float offset_pct, const Color8& c) {
    ss_ << tag::STOP << make_attr(attr::OFFSET, std::format("{}%", offset_pct))
        << make_attr(attr::STOP_COLOR, to_rgb(c))
        << make_attr(attr::STOP_OPACITY, opacity(c)) << tag::RECT_CLOSE;
}

void Canvas::end_linear_gradient() { 
    ss_ << tag::LINEAR_GRAD_CLOSE; 
}

void Canvas::start_group(const std::string& clip_path_id) {
    if (!clip_path_id.empty()) {
        ss_ << tag::GROUP_OPEN << make_attr(attr::CLIP_PATH, std::format("url(#{})", clip_path_id)) << tag::END_NEW_LINE;
    } else {
        ss_ << tag::GROUP_OPEN_RAW;
    }
}

void Canvas::end_group() { 
    ss_ << tag::GROUP_CLOSE; 
}

void Canvas::add_polygon(const std::vector<std::pair<float, float>>& pts, const Color8& fill, float opacity) {
    std::string points_str;
    for (size_t i = 0; i < pts.size(); ++i) {
        std::format_to(std::back_inserter(points_str), "{},{}", pts[i].first, pts[i].second);
        if (i < pts.size() - 1) points_str.push_back(' ');
    }
    ss_ << tag::POLYGON << make_attr(attr::POINTS, points_str)
        << make_attr(attr::FILL, to_rgb(fill)) << make_attr(attr::FILL_OPACITY, opacity)
        << make_attr(attr::STROKE, val::NONE) << tag::RECT_CLOSE;
}

void Canvas::add_line(float x1, float y1, float x2, float y2, const Color8& c, float width) {
    ss_ << tag::LINE 
        << make_attr(attr::X1, x1) << make_attr(attr::Y1, y1) 
        << make_attr(attr::X2, x2) << make_attr(attr::Y2, y2)
        << make_attr(attr::STROKE, to_rgb(c)) << make_attr(attr::STOP_OPACITY, opacity(c)) 
        << make_attr(attr::STROKE_WIDTH, width) << make_attr(attr::STROKE_LINECAP, val::ROUND) << tag::RECT_CLOSE;
}

void Canvas::add_circle(float cx, float cy, float r, const Color8& c) {
    ss_ << tag::CIRCLE << make_attr(attr::CX, cx) << make_attr(attr::CY, cy) << make_attr(attr::R, r)
        << make_attr(attr::FILL, to_rgb(c)) << make_attr(attr::STROKE, val::NONE) << tag::RECT_CLOSE;
}

void Canvas::add_text(float x, float y, const std::string& text, const Color8& c, int32_t size, const std::string& anchor) {
    ss_ << tag::TEXT_OPEN << make_attr(attr::X, x) << make_attr(attr::Y, y)
        << make_attr(attr::FILL, to_rgb(c)) << make_attr(attr::FONT_SIZE, size)
        << make_attr(attr::FONT_FAMILY, val::DEFAULT_FONT) << make_attr(attr::TEXT_ANCHOR, anchor)
        << make_attr(attr::DOM_BASELINE, val::MIDDLE) << ">" << escape(text) << tag::TEXT_CLOSE;
}

void Canvas::add_text_5x7(float x, float y, std::string_view text, const Color8& color, int32_t px_scale, int32_t anchor) {
    glm::vec2 sz = measure_text_5x7(text, px_scale);
    float xx = x;
    if (anchor == 1) xx -= sz.x * 0.5f;
    if (anchor == 2) xx -= sz.x;

    const int cw = 5 * px_scale;
    const int sp = 1 * px_scale;

    float pen_x = xx;
    uint8_t rows[7];
    for (char c : text) {
        if (!glyph_5x7(c, rows)) {
            pen_x += (float)(cw + sp);
            continue;
        }
        for (int ry = 0; ry < 7; ++ry) {
            for (int rx = 0; rx < 5; ++rx) {
                if (rows[ry] & (1u << (4 - rx))) {
                ss_ << tag::RECT_OPEN
                    << make_attr(attr::X, pen_x + static_cast<float>(rx * px_scale))
                    << make_attr(attr::Y, y + static_cast<float>(ry * px_scale))
                    << make_attr(attr::WIDTH, px_scale)
                    << make_attr(attr::HEIGHT, px_scale)
                    << make_attr(attr::FILL, to_rgb(color)) 
                    << tag::RECT_CLOSE;
                }
            }
        }
        pen_x += (float)(cw + sp);
    }
}

bool Canvas::save_svg(const std::string& path) {
    ss_ << SVG_CLOSE;
    ss_ << "\n";
    FileSystem::write(path, ss_.str()); 
    return true;
}

std::string Canvas::escape(std::string_view data) {
    std::string buffer;
    buffer.reserve(data.size() + 8);
    for (char ch : data) {
        switch (ch) {
            case '&':  buffer.append("&amp;");   break;
            case '\"': buffer.append("&quot;");  break;
            case '\'': buffer.append("&apos;");  break;
            case '<':  buffer.append("&lt;");    break;
            case '>':  buffer.append("&gt;");    break;
            default:   buffer.push_back(ch);     break;
        }
    }
    return buffer;
}

std::string Canvas::to_rgb(const Color8& c) {
    return std::format("rgb({},{},{})", c.get_r(), c.get_g(), c.get_b());
}

double Canvas::opacity(const Color8& c) {
    return static_cast<double>(c.get_a()) / 255.0;
}

}