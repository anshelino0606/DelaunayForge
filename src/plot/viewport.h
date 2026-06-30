#pragma once

#include "colorbar.h"
#include "geom/geom2d/types.h"
#include <glm/vec2.hpp>

namespace fem::plot {

struct Margins {
    int32_t left = 0;
    int32_t right = 0;
    int32_t top = 0;
    int32_t bottom = 0;
};

struct ViewportInitInfo {
    geom2d::BoundingBox* mesh_bbox = nullptr;;
    glm::vec2 view_size{0};
    int32_t scale = 0;
    bool include_colorbar = false;
    bool have_bounds = false;
    bool include_solution = false;
};

class Viewport {
public:
    Viewport(const ViewportInitInfo& init_info);

    [[nodiscard]] glm::vec2 to_px(double wx, double wy) const noexcept;
    [[nodiscard]] glm::vec2 to_px(const glm::dvec2& wpos) const noexcept;

    int32_t canvas_width() const noexcept { return canvas_width_; }
    int32_t canvas_height() const noexcept { return canvas_height_; }
    int32_t plot_x0() const noexcept { return plot_x0_; }
    int32_t plot_y0() const noexcept { return plot_y0_; }
    int32_t plot_width() const noexcept { return plot_width_; }
    int32_t plot_height() const noexcept { return plot_height_; }
    int32_t scale() const noexcept { return scale_; }
    bool has_colorbar() const noexcept { return show_colorbar_; }
    const Margins& margins() const noexcept { return margins_; }
    const Colorbar& colorbar() const noexcept { return colorbar_; }
    const geom2d::BoundingBox& world_bounds() const noexcept { return world_bounds_; }

private:
    int32_t scale_ = 0;
    bool show_colorbar_ = false;
    
    int32_t canvas_width_ = 0;
    int32_t canvas_height_ = 0;
    int32_t plot_width_ = 0;
    int32_t plot_height_ = 0;
    int32_t plot_x0_ = 0;
    int32_t plot_y0_ = 0;
    double world_w_ = 0;
    double world_h_ = 0;
    
    Margins margins_;
    Colorbar colorbar_;
    geom2d::BoundingBox world_bounds_;
};

}