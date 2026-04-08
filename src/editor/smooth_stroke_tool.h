#pragma once

#include "geom/delaunay_types.h"
#include "geom/smooth_stroke_config.h"

#include <vector>
#include <optional>
#include <glm/glm.hpp>

class Viewport;
struct ImDrawList;

namespace fem {

class SmoothStrokeTool {
public:
    void reset();
    bool active() const { return active_; }

    std::optional<std::vector<Point2D>> update(
        const Viewport& vp,
        bool hovered,
        ImDrawList* draw_list,
        const SmoothStrokeConfig& cfg);

private:
    bool active_ = false;
    std::vector<glm::dvec2> pts_screen_;
};

} // namespace fem
