#include "vector_canvas.h"
#include "utils.h"
#include "core/color.h"
#include "core/file_system/file_system.h"
#include <format>

namespace fem::plot {

VectorCanvas::VectorCanvas(int32_t width, int32_t height) : width_(width), height_(height) {
    ss_ << XML_HEADER << "\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\""
        << make_attr(Attr::WIDTH, width_) 
        << make_attr(Attr::HEIGHT, height_)
        << " viewBox=\"0 0 " << width_ << " " << height_ << "\">\n";
}

void VectorCanvas::add_rect(int32_t x, int32_t y, int32_t w, int32_t h, const std::string& fill, const std::string& stroke, float stroke_w) {
    ss_ << "  <rect" 
        << make_attr(Attr::X, x) << make_attr(Attr::Y, y) 
        << make_attr(Attr::WIDTH, w) << make_attr(Attr::HEIGHT, h)
        << make_attr(Attr::FILL, fill) << make_attr(Attr::STROKE, stroke) 
        << make_attr(Attr::STROKE_WIDTH, stroke_w) << "/>\n";
}

void VectorCanvas::start_defs() { 
    ss_ << "  <defs>\n"; 
}

void VectorCanvas::end_defs() { 
    ss_ << "  </defs>\n"; 
}

void VectorCanvas::add_clip_path(const std::string& id, int32_t x, int32_t y, int32_t w, int32_t h) {
    ss_ << "    <clipPath" << make_attr(Attr::ID, id) << "><rect"
        << make_attr(Attr::X, x) << make_attr(Attr::Y, y) 
        << make_attr(Attr::WIDTH, w) << make_attr(Attr::HEIGHT, h) << "/></clipPath>\n";
}

void VectorCanvas::start_linear_gradient(const std::string& id, float x1, float y1, float x2, float y2) {
    ss_ << "    <linearGradient" << make_attr(Attr::ID, id)
        << make_attr(Attr::X1, x1) << make_attr(Attr::Y1, y1) 
        << make_attr(Attr::X2, x2) << make_attr(Attr::Y2, y2) << ">\n";
}

void VectorCanvas::add_gradient_stop(float offset_pct, const Color8& c) {
    ss_ << "      <stop" << make_attr(Attr::OFFSET, std::to_string(offset_pct) + "%")
        << make_attr(Attr::STOP_COLOR, to_rgb(c))
        << make_attr(Attr::STOP_OPACITY, opacity(c)) << "/>\n";
}

void VectorCanvas::end_linear_gradient() { 
    ss_ << "    </linearGradient>\n"; 
}

void VectorCanvas::start_group(const std::string& clip_path_id) {
    if (!clip_path_id.empty()) {
        ss_ << "  <g" << make_attr(Attr::CLIP_PATH, "url(#" + clip_path_id + ")") << ">\n";
    } else {
        ss_ << "  <g>\n";
    }
}
void VectorCanvas::end_group() { 
    ss_ << "  </g>\n"; 
}

void VectorCanvas::add_polygon(const std::vector<std::pair<float, float>>& pts, const std::string& fill, float opacity) {
    std::string points_str;
    for (size_t i = 0; i < pts.size(); ++i) {
        points_str += std::to_string(pts[i].first) + "," + std::to_string(pts[i].second) + (i == pts.size() - 1 ? "" : " ");
    }
    ss_ << "    <polygon" << make_attr(Attr::POINTS, points_str)
        << make_attr(Attr::FILL, fill) << make_attr(Attr::FILL_OPACITY, opacity)
        << make_attr(Attr::STROKE, Val::NONE) << "/>\n";
}

void VectorCanvas::add_line(float x1, float y1, float x2, float y2, const Color8& c, float width) {
    ss_ << "    <line" 
        << make_attr(Attr::X1, x1) << make_attr(Attr::Y1, y1) 
        << make_attr(Attr::X2, x2) << make_attr(Attr::Y2, y2)
        << make_attr(Attr::STROKE, to_rgb(c)) << make_attr(Attr::STOP_OPACITY, opacity(c)) 
        << make_attr(Attr::STROKE_WIDTH, width) << make_attr(Attr::STROKE_LINECAP, Val::ROUND) << "/>\n";
}

void VectorCanvas::add_circle(float cx, float cy, float r, const Color8& c) {
    ss_ << "    <circle" << make_attr(Attr::CX, cx) << make_attr(Attr::CY, cy) << make_attr(Attr::R, r)
        << make_attr(Attr::FILL, to_rgb(c)) << make_attr(Attr::STROKE, Val::NONE) << "/>\n";
}

void VectorCanvas::add_text(float x, float y, const std::string& text, const Color8& c, int32_t size, const std::string& anchor) {
    ss_ << "  <text" << make_attr(Attr::X, x) << make_attr(Attr::Y, y)
        << make_attr(Attr::FILL, to_rgb(c)) << make_attr(Attr::FONT_SIZE, size)
        << make_attr(Attr::FONT_FAMILY, Val::DEFAULT_FONT) << make_attr(Attr::TEXT_ANCHOR, anchor)
        << make_attr(Attr::DOM_BASELINE, Val::MIDDLE) << ">" << escape(text) << "</text>\n";
}

void VectorCanvas::add_text_5x7(float x, float y, std::string_view text, const Color8& color, int32_t px_scale, int32_t anchor) {
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
                    ss_ << "  <rect x=\"" << (pen_x + (float)(rx * px_scale))
                       << "\" y=\"" << (y + (float)(ry * px_scale))
                       << "\" width=\"" << px_scale
                       << "\" height=\"" << px_scale
                       << "\" fill=\"" << to_rgb(color) << "\"/>\n";
                }
            }
        }
        pen_x += (float)(cw + sp);
    }
}

bool VectorCanvas::save_svg(const std::string& path) {
    ss_ << SVG_CLOSE;
    ss_ << "\n";
    FileSystem::write(path, ss_.str()); 
    return true;
}

std::string VectorCanvas::escape(std::string_view data) {
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

std::string VectorCanvas::to_rgb(const Color8& c) {
    return std::format("rgb({},{},{})", c.get_r(), c.get_g(), c.get_b());
}

double VectorCanvas::opacity(const Color8& c) {
    return static_cast<double>(c.get_a()) / 255.0;
}

std::string VectorCanvas::make_attr(std::string_view name, std::string_view value) const {
    return std::format(" {}=\"{}\"", name, value);
}

std::string VectorCanvas::make_attr(std::string_view name, double value) const {
    return std::format(" {}=\"{}\"", name, value);
}

}