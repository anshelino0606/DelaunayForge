#include "svg.h"
#include "svg_canvas.h"
#include "colorbar.h"
#include "viewport.h"
#include "utils.h"
#include "theme.h"
#include "core/color.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "math/pde/pde_component.h"
#include "math/boundary_condition.h"
#include <algorithm>
#include <cmath>

namespace fem::plot {

bool export_svg(const std::string& absolute_path, const SceneData& scene) {
    if (!scene.last_mesh) {
        return false;
    }
    
    const DelaunayTriangulationResult& R = scene.last_mesh->triangulation_result();
    if (scene.viewport_size.x <= 1.0f || scene.viewport_size.y <= 1.0f || R.points.empty()) {
        return false;
    }

    geom2d::BoundingBox mesh_bbox;
    for (const Point2D& pt : R.points) {
        mesh_bbox.update(static_cast<glm::dvec2>(pt));
    }

    double u_min = 0.0;
    double u_max = 0.0;
    bool have_bounds = false;
    
    if (scene.pde) {
        std::pair<double, double> bounds = scene.pde->get_global_bounds();
        u_min = bounds.first; 
        u_max = bounds.second;
        have_bounds = (u_min <= u_max);
    }

    const int32_t scale = std::max(1, scene.settings.scale_factor);

    ViewportInitInfo init_info{
        .mesh_bbox        = &mesh_bbox,
        .view_size        = scene.viewport_size,
        .scale            = scale,
        .include_colorbar = scene.settings.include_colorbar,
        .have_bounds      = have_bounds,
        .include_solution = scene.settings.include_solution
    };
    Viewport vp(init_info);

    const bool dark = (scene.settings.theme == ExportSettings::Theme::Dark);
    const Color8 bg_color = dark ? COLOR_DARK_BG : COLOR_LIGHT_BG;
    const Color8 fg_color = dark ? COLOR_DARK_FG : COLOR_LIGHT_FG;

    svg::Canvas canvas(vp.canvas_width(), vp.canvas_height());

    canvas.add_rect(0, 0, vp.canvas_width(), vp.canvas_height(), &bg_color);

    canvas.start_defs();
    canvas.add_clip_path("plotClip", vp.plot_x0(), vp.plot_y0(), vp.plot_width(), vp.plot_height());
    
    if (vp.has_colorbar()) {
        canvas.start_linear_gradient("cbGrad", 0.0f, 1.0f, 0.0f, 0.0f);
        constexpr int32_t kColorbarStops = 16;
        for (int32_t i = 0; i <= kColorbarStops; ++i) {
            float pct = (static_cast<float>(i) / static_cast<float>(kColorbarStops));
            double u_val = u_min + static_cast<double>(pct) * (u_max - u_min);
            Color8 stop_color = color_for_u(u_min, u_max, u_val, have_bounds);
            canvas.add_gradient_stop(pct * 100.0f, stop_color);
        }
        canvas.end_linear_gradient();
    }
    canvas.end_defs();

    if (scene.settings.include_axes) {
        const float frame_w = std::max(1.0f, FRAME_THICKNESS_MOD * static_cast<float>(scale));
        canvas.add_rect(vp.plot_x0(), vp.plot_y0(), vp.plot_width(), vp.plot_height(), 
            nullptr, &fg_color, frame_w);
    }

    canvas.start_group("plotClip");

    if (scene.settings.include_solution && scene.pde && have_bounds) {
        const DifferentialEquationSolution& sol = scene.pde->solution(scene.last_mesh);
        if (sol.is_ready() && sol.solution_u.size() == R.points.size()) {
            std::vector<std::pair<float, float>> poly_pts(3);
            for (const Tri& tri : R.triangles) {
                if (!tri.valid) continue;
                
                double u_sum = sol.solution_u[tri.v[0]] + sol.solution_u[tri.v[1]] + sol.solution_u[tri.v[2]];
                double uavg = u_sum / TRI_AVERAGE_DIVISOR;
                Color8 fill_color = color_for_u(u_min, u_max, uavg, have_bounds);
                
                glm::vec2 p0 = vp.to_px(R.points[tri.v[0]]);
                glm::vec2 p1 = vp.to_px(R.points[tri.v[1]]);
                glm::vec2 p2 = vp.to_px(R.points[tri.v[2]]);
                
                poly_pts[0] = { p0.x, p0.y };
                poly_pts[1] = { p1.x, p1.y };
                poly_pts[2] = { p2.x, p2.y };
                
                canvas.add_polygon(poly_pts, fill_color, 0.784f);
            }
        }
    }

    if (scene.settings.include_mesh) {
        const float thin = std::max(0.6f, (scale > 2) ? static_cast<float>(scale) * 0.45f : 1.0f);
        const float thick = std::max(1.2f, static_cast<float>(scale));
        const bool solution_visible = scene.settings.include_solution && scene.pde && have_bounds;
        
        Color8 edge_col = dark ? MESH_DARK_HIDDEN_EDGE : MESH_LIGHT_HIDDEN_EDGE;
        if (solution_visible) {
            edge_col = dark ? MESH_DARK_VISIBLE_EDGE : MESH_LIGHT_VISIBLE_EDGE;
        }
        const Color8 boundary_col = dark ? MESH_DARK_BOUNDARY : MESH_LIGHT_BOUNDARY;
        
        for (const EdgeInfo& e : R.edges) {
            if (!e.valid_vertices(R.points.size())) continue;
            
            glm::vec2 pA = vp.to_px(R.points[e.a]); 
            glm::vec2 pB = vp.to_px(R.points[e.b]);
            Color8 current_color = e.on_boundary ? boundary_col : edge_col;
            float current_width = e.on_boundary ? thick : thin;
            
            canvas.add_line(pA.x, pA.y, pB.x, pB.y, current_color, current_width);
        }
    }

    if (scene.settings.include_boundary_conditions && scene.last_mesh) {
        const float bc_thickness = std::max(1.5f, static_cast<float>(scale) * 1.2f);
        for (BoundaryCondition* bc : scene.last_mesh->boundary_conditions()) {
            size_t bc_idx = (size_t)bc->type().value;
            Color8 color = dark ? BC_DARK[bc_idx] : BC_LIGHT[bc_idx];
            
            for (int32_t eid : bc->edge_ids()) {
                if ((size_t)eid >= R.edges.size() || !R.edges[eid].valid_vertices(R.points.size())) continue;
                
                glm::vec2 pA = vp.to_px(R.points[R.edges[eid].a]); 
                glm::vec2 pB = vp.to_px(R.points[R.edges[eid].b]);
                
                canvas.add_line(pA.x, pA.y, pB.x, pB.y, color, bc_thickness);
            }
        }
    }

    if (scene.settings.include_points) {
        const float r = std::max(1.0f, scene.point_radius * static_cast<float>(scale) * POINT_RADIUS_SCALE);
        const Color8 in_col = dark ? NODE_DARK_INTERNAL : NODE_LIGHT_INTERNAL;
        const Color8 bd_col = dark ? NODE_DARK_BOUNDARY : NODE_LIGHT_BOUNDARY;
        
        for (const Point2D& pt : R.points) {
            glm::vec2 p = vp.to_px(pt);
            Color8 current_color = pt.on_boundary ? bd_col : in_col;
            canvas.add_circle(p.x, p.y, r, current_color);
        }
    }

    canvas.end_group();

    if (scene.settings.include_axes) {
        const float tick_len = static_cast<float>(5 * scale);
        const int32_t label_scale = std::max(1, scale);
        
        int32_t x0 = vp.plot_x0();
        int32_t y0 = vp.plot_y0();
        int32_t h  = vp.plot_height();

        auto x_ticks = plot::nice_ticks(vp.world_bounds().mins.x, vp.world_bounds().maxs.x, TICK_COUNT_TARGET);
        for (double wx : x_ticks) {
            float x_pos = vp.to_px(wx, vp.world_bounds().mins.y).x;
            float y_pos = static_cast<float>(y0 + h);
            
            canvas.add_line(x_pos, y_pos, x_pos, y_pos + tick_len, fg_color, static_cast<float>(scale));
            canvas.add_text_5x7(x_pos, y_pos + tick_len + static_cast<float>(4 * scale), plot::fmt_tick(wx), fg_color, label_scale, 1);
        }
        
        auto y_ticks = plot::nice_ticks(vp.world_bounds().mins.y, vp.world_bounds().maxs.y, TICK_COUNT_TARGET);
        for (double wy : y_ticks) {
            float y_pos = vp.to_px(vp.world_bounds().mins.x, wy).y;
            float x_pos = static_cast<float>(x0);
            
            canvas.add_line(x_pos, y_pos, x_pos - tick_len, y_pos, fg_color, static_cast<float>(scale));
            canvas.add_text_5x7(x_pos - tick_len - static_cast<float>(4 * scale), y_pos - static_cast<float>(std::lround(3.5f * label_scale)), plot::fmt_tick(wy), fg_color, label_scale, 2);
        }
    }

    if (vp.has_colorbar()) {
        const Colorbar& cb = vp.colorbar(); 
        const Rect& rect = cb.rect();
        
        canvas.add_gradient_rect(rect.x, rect.y, rect.w, rect.h, "url(#cbGrad)", &fg_color, static_cast<float>(scale));

        const float text_x = static_cast<float>(rect.x + rect.w) + static_cast<float>(std::lround(cb.label_gap()));
        const int32_t label_scale = cb.bitmap_label_scale();
        const std::string u_min_label = plot::fmt_tick(u_min);
        float label_y_offset = static_cast<float>(std::lround(plot::measure_text_5x7(u_min_label, label_scale).y));
        
        canvas.add_text_5x7(text_x, static_cast<float>(rect.y), plot::fmt_tick(u_max), fg_color, label_scale, 0);
        canvas.add_text_5x7(text_x, static_cast<float>(rect.y + rect.h) - label_y_offset, u_min_label, fg_color, label_scale, 0);
    }

    if (scene.settings.include_bc_legend && scene.settings.include_boundary_conditions && scene.last_mesh) {        
        const int32_t font_px = 11 * scale;
        const float legend_x  = static_cast<float>(10 * scale);
        const float legend_y  = static_cast<float>(vp.plot_y0() + vp.plot_height() - 10 * scale);
        const float line_h    = static_cast<float>(18 * scale);
        const float line_w    = static_cast<float>(25 * scale);
        const float thickness = std::max(2.0f, 2.5f * static_cast<float>(scale));
        
        std::vector<BoundaryConditionType> bc_types;
        for (BoundaryCondition* bc : scene.last_mesh->boundary_conditions()) {
            auto check_lambda = [&](const BoundaryConditionType& existing) { 
                return existing.value == bc->type().value; 
            };
            auto found_it = std::find_if(bc_types.begin(), bc_types.end(), check_lambda);

            if (found_it == bc_types.end()) {
                bc_types.push_back(bc->type());
            }
        }
        
        float y_offset = 0.0f;
        for (BoundaryConditionType bc_type : bc_types) {
            float current_y = legend_y - y_offset; 

            size_t bc_idx = (size_t)bc_type.value;
            Color8 color = dark ? BC_DARK[bc_idx] : BC_LIGHT[bc_idx];
            std::string name = std::string(bc_type.to_string());
            
            canvas.add_line(legend_x, current_y, legend_x + line_w, current_y, color, thickness);
            
            canvas.add_text(legend_x + line_w + static_cast<float>(6 * scale), 
                current_y, name, fg_color, font_px);
            
            y_offset += line_h;
        }
    }

    return canvas.save_svg(absolute_path);
}

}