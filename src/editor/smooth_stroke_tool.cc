#include "smooth_stroke_tool.h"
#include "editor/viewport.h"
#include "math/curve.h"
#include "delaunay_types.h"
#include <utility>
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "log_categories.h"


static double dist2(const glm::dvec2& a, const glm::dvec2& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return dx*dx + dy*dy;
}

namespace fem {

void SmoothStrokeTool::reset() {
    active_ = false;
    pts_screen_.clear();
}

std::optional<std::vector<Point2D>> SmoothStrokeTool::update(
    const Viewport& vp,
    bool hovered,
    ImDrawList* draw_list,
    const SmoothStrokeConfig& cfg)
{
    ImGuiIO& io = ImGui::GetIO();

    if (!active_) {
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            active_ = true;
            pts_screen_.clear();
            pts_screen_.emplace_back((double)io.MousePos.x, (double)io.MousePos.y);
        }
        return std::nullopt;
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const glm::dvec2 p((double)io.MousePos.x, (double)io.MousePos.y);

        if (pts_screen_.empty()) {
            pts_screen_.push_back(p);
        } else {
            if (dist2(p, pts_screen_.back()) >= cfg.screen_step_px * cfg.screen_step_px) {
                pts_screen_.push_back(p);
            }
        }
    }

    if (draw_list && pts_screen_.size() >= 2) {
        std::vector<ImVec2> visual_pts;
        visual_pts.reserve(pts_screen_.size());
        for(const auto& p : pts_screen_) {
            visual_pts.emplace_back((float)p.x, (float)p.y);
        }
        draw_list->AddPolyline(visual_pts.data(), (int)visual_pts.size(), IM_COL32(0, 255, 0, 255), false, 2.0f);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        active_ = false;

        if (pts_screen_.size() < 3) {
            pts_screen_.clear();
            return std::nullopt;
        }

        const double z = std::max(1e-6, (double)vp.zoom);
        const double close_thr_world = cfg.close_threshold_px / z;
        const double min_dist_world  = cfg.min_dist_px        / z;

        std::vector<glm::dvec2> world_ctrl;
        world_ctrl.reserve(pts_screen_.size());
        for (const auto& sp : pts_screen_) {
            glm::dvec2 wp = vp.to_world(ImVec2((float)sp.x, (float)sp.y));
            world_ctrl.emplace_back(wp.x, wp.y);
        }

        if (world_ctrl.size() >= 2) {
            const glm::dvec2 a = world_ctrl.front();
            const glm::dvec2 b = world_ctrl.back();
            const glm::dvec2 d = a - b;
            if (d.x*d.x + d.y*d.y > close_thr_world * close_thr_world) {
                world_ctrl.push_back(world_ctrl.front());
            }
        }

        std::vector<Point2D> result_points = make_closed_smooth_loop(
            std::move(world_ctrl),
            cfg.boundary_sample_count,
            close_thr_world,
            min_dist_world,
            cfg.catmull_per_seg
        );

        if (result_points.size() < 3) {
            pts_screen_.clear();
            return std::nullopt;
        }
        pts_screen_.clear();
        LOGT_DEBUG(LogMath, "Result points size: %d", result_points.size());
        return result_points;
    }

    return std::nullopt;
}

} // namespace fem