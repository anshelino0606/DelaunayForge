#include "png.h"
#include "bitmap_canvas.h"
#include "colorbar.h"
#include "viewport.h"
#include "utils.h"
#include "theme.h"
#include "core/color.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "math/pde/pde_component.h"
#include "math/boundary_condition.h"

namespace fem::plot {

bool export_png(const std::string& absolute_path, const SceneData& scene) {
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

    BitmapCanvas canvas(vp.canvas_width(), vp.canvas_height(), bg_color);

    if (scene.settings.include_solution && scene.pde && have_bounds) {
        const DifferentialEquationSolution& sol = scene.pde->solution(scene.last_mesh);
        
        if (sol.is_ready() && sol.solution_u.size() == R.points.size()) {
            for (const Tri& tri : R.triangles) {
                if (!tri.valid) continue;
                
                double u_sum = sol.solution_u[tri.v[0]] + sol.solution_u[tri.v[1]] + sol.solution_u[tri.v[2]];
                double uavg = u_sum / TRI_AVERAGE_DIVISOR;
                
                glm::vec2 p0 = vp.to_px(R.points[tri.v[0]]);
                glm::vec2 p1 = vp.to_px(R.points[tri.v[1]]);
                glm::vec2 p2 = vp.to_px(R.points[tri.v[2]]);
                
                canvas.fill_triangle(p0, p1, p2, color_for_u(u_min, u_max, uavg, have_bounds));
            }
        }
    }

    if (scene.settings.include_mesh) {
        const int32_t thin = std::max(1, (scale > 2) ? scale / 2 : 1);
        const int32_t thick = std::max(1, scale);
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
            int32_t current_width = e.on_boundary ? thick : thin;
            
            canvas.draw_line((int)pA.x, (int)pA.y, (int)pB.x, (int)pB.y, current_color, current_width);
        }
    }

    if (scene.settings.include_boundary_conditions && scene.last_mesh) {
        const int32_t bc_thickness = std::max(2, static_cast<int32_t>(BC_THICKNESS_MOD * scale));
        for (BoundaryCondition* bc : scene.last_mesh->boundary_conditions()) {
            size_t bc_idx = (size_t)bc->type().value;
            Color8 color = dark ? BC_DARK[bc_idx] : BC_LIGHT[bc_idx];
            
            for (int32_t eid : bc->edge_ids()) {
                if ((size_t)eid >= R.edges.size() || !R.edges[eid].valid_vertices(R.points.size())) continue;
                
                glm::vec2 pA = vp.to_px(R.points[R.edges[eid].a]); 
                glm::vec2 pB = vp.to_px(R.points[R.edges[eid].b]);
                
                canvas.draw_line((int)pA.x, (int)pA.y, (int)pB.x, (int)pB.y, color, bc_thickness);
            }
        }
    }

    if (scene.settings.include_points) {
        const int32_t r = std::max(1, static_cast<int32_t>(std::lround(scene.point_radius * (float)scale * POINT_RADIUS_SCALE)));
        const Color8 in_col = dark ? NODE_DARK_INTERNAL : NODE_LIGHT_INTERNAL;
        const Color8 bd_col = dark ? NODE_DARK_BOUNDARY : NODE_LIGHT_BOUNDARY;
        
        for (const Point2D& pt : R.points) {
            glm::vec2 p = vp.to_px(pt);
            Color8 current_color = pt.on_boundary ? bd_col : in_col;
            
            canvas.draw_circle((int)p.x, (int)p.y, r, current_color);
        }
    }

    if (scene.settings.include_axes) {
        const int32_t frame_w = std::max(1, static_cast<int32_t>(FRAME_THICKNESS_MOD * scale));
        const int32_t tick_len = 5 * scale;
        const int32_t label_scale = std::max(1, scale);
        
        int32_t x0 = vp.plot_x0();
        int32_t y0 = vp.plot_y0();
        int32_t w  = vp.plot_width();
        int32_t h  = vp.plot_height();
        
        canvas.draw_line(x0, y0, x0 + w, y0, fg_color, frame_w);
        canvas.draw_line(x0 + w, y0, x0 + w, y0 + h, fg_color, frame_w);
        canvas.draw_line(x0 + w, y0 + h, x0, y0 + h, fg_color, frame_w);
        canvas.draw_line(x0, y0 + h, x0, y0, fg_color, frame_w);

        auto x_ticks = plot::nice_ticks(vp.world_bounds().mins.x, vp.world_bounds().maxs.x, TICK_COUNT_TARGET);
        for (double wx : x_ticks) {
            int32_t x_pos = static_cast<int32_t>(vp.to_px(wx, vp.world_bounds().mins.y).x);
            int32_t y_pos = y0 + h;
            
            canvas.draw_line(x_pos, y_pos, x_pos, y_pos + tick_len, fg_color, scale);
            canvas.draw_text_5x7(x_pos, y_pos + tick_len + 4 * scale, plot::fmt_tick(wx), fg_color, label_scale, 1);
        }
        
        auto y_ticks = plot::nice_ticks(vp.world_bounds().mins.y, vp.world_bounds().maxs.y, TICK_COUNT_TARGET);
        for (double wy : y_ticks) {
            int32_t y_pos = static_cast<int32_t>(vp.to_px(vp.world_bounds().mins.x, wy).y);
            int32_t x_pos = x0;
            
            canvas.draw_line(x_pos, y_pos, x_pos - tick_len, y_pos, fg_color, scale);
            canvas.draw_text_5x7(x_pos - tick_len - 4 * scale, y_pos - static_cast<int32_t>(std::lround(3.5f * label_scale)), plot::fmt_tick(wy), fg_color, label_scale, 2);
        }
    }

    if (vp.has_colorbar()) {
        const Colorbar& cb = vp.colorbar(); 
        const Rect& rect = cb.rect();
        
        for (int32_t y = 0; y < rect.h; ++y) {
            double normalized_y = 1.0 - (double)y / (rect.h - 1);
            double u_val = u_min + normalized_y * (u_max - u_min);
            Color8 sample_color = color_for_u(u_min, u_max, u_val, have_bounds);
            
            for (int32_t x = 0; x < rect.w; ++x) {
                canvas.set_pixel(rect.x + x, rect.y + y, sample_color);
            }
        }
        
        canvas.draw_line(rect.x, rect.y, rect.x + rect.w, rect.y, fg_color, scale);
        canvas.draw_line(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h, fg_color, scale);
        canvas.draw_line(rect.x + rect.w, rect.y + rect.h, rect.x, rect.y + rect.h, fg_color, scale);
        canvas.draw_line(rect.x, rect.y + rect.h, rect.x, rect.y, fg_color, scale);

        const int32_t text_x = rect.x + rect.w + static_cast<int32_t>(std::lround(cb.label_gap()));
        const int32_t label_scale = cb.bitmap_label_scale();
        const std::string u_min_label = plot::fmt_tick(u_min);
        int32_t label_y_offset = static_cast<int32_t>(std::lround(plot::measure_text_5x7(u_min_label, label_scale).y));
        
        canvas.draw_text_5x7(text_x, rect.y, plot::fmt_tick(u_max), fg_color, label_scale, 0);
        canvas.draw_text_5x7(text_x, rect.y + rect.h - label_y_offset, u_min_label, fg_color, label_scale, 0);
    }

    if (scene.settings.include_bc_legend && scene.settings.include_boundary_conditions && scene.last_mesh) {        
        const int32_t legend_x = 10 * scale;
        const int32_t legend_y = vp.plot_y0() + vp.plot_height() - 10 * scale;
        const int32_t line_h   = 12 * scale;
        const int32_t line_w   = 20 * scale;
        const int32_t text_w_scale = std::max(1, scale);
        const int32_t thickness    = std::max(2, static_cast<int32_t>(2.5f * scale));
        
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
        
        int32_t y_offset = 0;
        for (BoundaryConditionType bc_type : bc_types) {
            int32_t current_y = legend_y - y_offset; 

            size_t bc_idx = (size_t)bc_type.value;
            Color8 color = dark ? BC_DARK[bc_idx] : BC_LIGHT[bc_idx];
            const char* name = bc_type.to_string().data();
            
            canvas.draw_line(legend_x, current_y, legend_x + line_w, current_y, color, thickness);
            canvas.draw_text_5x7(legend_x + line_w + 5 * scale, current_y - 3 * text_w_scale, name, fg_color, text_w_scale, 0);
            
            y_offset += line_h;
        }
    }

    return canvas.save_png(absolute_path);
}

}