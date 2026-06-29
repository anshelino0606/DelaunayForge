#include "colorbar.h"
#include <algorithm>
#include <cmath>

namespace fem::plot {

namespace {

// TODO: Move these layout constants to a config file in the future.
constexpr float DEFAULT_HEIGHT_RATIO       = 0.42f;
constexpr float DEFAULT_WIDTH_RATIO        = 0.055f;
constexpr float DEFAULT_X_GAP_RATIO        = 0.8f;
constexpr float DEFAULT_Y_OFFSET_RATIO     = 0.25f;
constexpr float DEFAULT_LABEL_GAP_RATIO    = 0.45f;
constexpr float DEFAULT_RIGHT_MARGIN_RATIO = 2.8f;
constexpr float DEFAULT_SVG_FONT_RATIO     = 0.6f;
constexpr float DEFAULT_BITMAP_LABEL_RATIO = 10.0f;

constexpr float MIN_HEIGHT_BASE            = 180.0f;
constexpr float MAX_HEIGHT_BASE            = 320.0f;
constexpr float MIN_WIDTH_BASE             = 24.0f;
constexpr float MAX_WIDTH_BASE             = 48.0f;
constexpr float MIN_X_GAP_BASE             = 18.0f;
constexpr float MIN_Y_OFFSET_BASE          = 10.0f;
constexpr float MIN_LABEL_GAP_BASE         = 10.0f;
constexpr float MIN_RIGHT_MARGIN_BASE      = 110.0f;
constexpr float MIN_SVG_FONT_BASE          = 14.0f;

}

void Colorbar::compute_layout(float plot_height, int32_t scale) {
    const float scale_f = static_cast<float>(std::max(scale, 1));
    
    height_    = std::clamp(plot_height * DEFAULT_HEIGHT_RATIO, MIN_HEIGHT_BASE * scale_f, MAX_HEIGHT_BASE * scale_f);
    width_     = std::clamp(plot_height * DEFAULT_WIDTH_RATIO, MIN_WIDTH_BASE * scale_f, MAX_WIDTH_BASE * scale_f);
    x_gap_     = std::max(MIN_X_GAP_BASE * scale_f, width_ * DEFAULT_X_GAP_RATIO);
    y_offset_  = std::max(MIN_Y_OFFSET_BASE * scale_f, width_ * DEFAULT_Y_OFFSET_RATIO);
    label_gap_ = std::max(MIN_LABEL_GAP_BASE * scale_f, width_ * DEFAULT_LABEL_GAP_RATIO);
    
    right_margin_       = static_cast<int32_t>(std::ceil(x_gap_ + width_ + std::max(MIN_RIGHT_MARGIN_BASE * scale_f, width_ * DEFAULT_RIGHT_MARGIN_RATIO)));
    svg_font_px_        = static_cast<int32_t>(std::lround(std::max(MIN_SVG_FONT_BASE * scale_f, width_ * DEFAULT_SVG_FONT_RATIO)));
    bitmap_label_scale_ = std::max(scale + 1, static_cast<int32_t>(std::lround(width_ / DEFAULT_BITMAP_LABEL_RATIO)));

    rect_.w = static_cast<int32_t>(std::lround(width_));
    rect_.h = static_cast<int32_t>(std::lround(height_));
}

void Colorbar::place(int32_t plot_x0, int32_t plot_width, int32_t plot_y0) {
    rect_.x = plot_x0 + plot_width + static_cast<int32_t>(std::lround(x_gap_));
    rect_.y = plot_y0 + static_cast<int32_t>(std::lround(y_offset_));
}
}