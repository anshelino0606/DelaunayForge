#pragma once

#include "core/types.h"

namespace fem::plot {

class Colorbar {
public:
    Colorbar() = default;

    void compute_layout(float plot_height, int32_t scale);
    void place(int32_t plot_x0, int32_t plot_width, int32_t plot_y0);

    const Rect& rect() const { return rect_; }
    int32_t right_margin() const noexcept { return right_margin_; }
    float label_gap()  const noexcept { return label_gap_; }
    int32_t bitmap_label_scale() const noexcept { return bitmap_label_scale_; }
    int32_t svg_font_px()  const noexcept { return svg_font_px_; }

private:
    Rect rect_;

    float width_  = 0.0f;
    float height_ = 0.0f;
    float x_gap_  = 0.0f;
    float y_offset_ = 0.0f;
    float label_gap_ = 0.0f;
    int32_t right_margin_ = 0;
    int32_t svg_font_px_  = 0;
    int32_t bitmap_label_scale_ = 1;
};

}