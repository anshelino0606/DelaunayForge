#include "viewport.h"
#include <cmath>

namespace fem::plot {

namespace {

// TODO: Move these layout constants to a config file in the future.
constexpr double GEOMETRIC_PADDING_RATIO = 0.03;
constexpr double MIN_WORLD_DIMENSION     = 1e-12;

constexpr int32_t BASE_MARGIN_LEFT   = 80;
constexpr int32_t BASE_MARGIN_BOTTOM = 60;
constexpr int32_t BASE_MARGIN_TOP    = 30;
constexpr int32_t BASE_MARGIN_RIGHT  = 30;

constexpr float   MIN_PLOT_HEIGHT    = 64.0f;
constexpr int32_t MIN_PLOT_DIMENSION = 64;

constexpr double ASPECT_DIVISOR_SCALE = 2.0;

}

Viewport::Viewport(const ViewportInitInfo& init_info) {
    assert(init_info.mesh_bbox);

    scale_ = init_info.scale;
    show_colorbar_ = init_info.include_colorbar && init_info.have_bounds && init_info.include_solution;

    const double pad_x = std::max(1.0, init_info.mesh_bbox->dx() * GEOMETRIC_PADDING_RATIO);
    const double pad_y = std::max(1.0, init_info.mesh_bbox->dy() * GEOMETRIC_PADDING_RATIO);
    
    world_bounds_.mins = glm::dvec2(init_info.mesh_bbox->mins.x - pad_x, init_info.mesh_bbox->mins.y - pad_y);
    world_bounds_.maxs = glm::dvec2(init_info.mesh_bbox->maxs.x + pad_x, init_info.mesh_bbox->maxs.y + pad_y);

    world_w_ = std::max(MIN_WORLD_DIMENSION, world_bounds_.dx());
    world_h_ = std::max(MIN_WORLD_DIMENSION, world_bounds_.dy());
    const double world_aspect = world_w_ / world_h_;

    margins_.left   = BASE_MARGIN_LEFT   * scale_;
    margins_.bottom = BASE_MARGIN_BOTTOM * scale_;
    margins_.top    = BASE_MARGIN_TOP    * scale_;
    margins_.right  = BASE_MARGIN_RIGHT  * scale_;

    if (show_colorbar_) {
        const float estimated_plot_h = std::max(MIN_PLOT_HEIGHT, (init_info.view_size.y * static_cast<float>(scale_)) - margins_.top - margins_.bottom);
        colorbar_.compute_layout(estimated_plot_h, scale_);
        margins_.right = std::max(margins_.right, colorbar_.right_margin());
    }

    int32_t base_plot_w = std::max(MIN_PLOT_DIMENSION, static_cast<int32_t>(std::lround(init_info.view_size.x * static_cast<float>(scale_))) - margins_.left - margins_.right);
    int32_t base_plot_h = std::max(MIN_PLOT_DIMENSION, static_cast<int32_t>(std::lround(init_info.view_size.y * static_cast<float>(scale_))) - margins_.top - margins_.bottom);
    
    canvas_width_  = base_plot_w + margins_.left + margins_.right;
    canvas_height_ = base_plot_h + margins_.top + margins_.bottom;

    const double screen_aspect = static_cast<double>(base_plot_w) / static_cast<double>(base_plot_h);
    if (world_aspect > screen_aspect) {
        plot_width_  = base_plot_w;
        plot_height_ = static_cast<int32_t>(std::lround(plot_width_ / world_aspect));
    } else {
        plot_height_ = base_plot_h;
        plot_width_  = static_cast<int32_t>(std::lround(plot_height_ * world_aspect));
    }

    plot_x0_ = margins_.left + (base_plot_w - plot_width_) / static_cast<int32_t>(ASPECT_DIVISOR_SCALE);
    plot_y0_ = margins_.top + (base_plot_h - plot_height_) / static_cast<int32_t>(ASPECT_DIVISOR_SCALE);

    if (show_colorbar_) {
        colorbar_.compute_layout(static_cast<float>(plot_height_), scale_);
        colorbar_.place(plot_x0_, plot_width_, plot_y0_);
    }
}

glm::vec2 Viewport::to_px(double wx, double wy) const noexcept {
    const double tx = (wx - world_bounds_.mins.x) / world_w_;
    const double ty = (world_bounds_.maxs.y - wy) / world_h_;
    return glm::vec2(
        static_cast<float>(plot_x0_ + tx * plot_width_),
        static_cast<float>(plot_y0_ + ty * plot_height_)
    );
}

glm::vec2 Viewport::to_px(const glm::dvec2& wpos) const noexcept {
    return to_px(wpos.x, wpos.y);
}

}