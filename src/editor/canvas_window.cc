#include "canvas_window.h"
#include "planar_mesh_generator_window.h"
#include "core/entity/entity.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "geom/planar_mesh/planar_mesh_boundary_base.h"
#include "geom/planar_mesh/planar_mesh_outer_boundary.h"
#include "geom/planar_mesh/planar_mesh_inner_boundary.h"
#include "geom/triangulation/triangulation_session_events.h"
#include "math/boundary_condition.h"
#include "math/bc_group_manager.h"
#include "math/differential_equation_solution.h"
#include "math/pde/pde_component.h"
#include "math/entities/planar_math_entity.h"
#include "tools/bc_utils.h"
#include "log_categories.h"
#include "core/file_system/file_system.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <format>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "math/curve.h"

namespace fem {

namespace {

inline std::string fmt_no_plus(double x) {
    std::string s = std::format("{:.6g}", x);
    if (auto p = s.find("e+"); p != std::string::npos) s.erase(p + 1, 1);
    return s;
}

// Compute "nice" rounded tick values a la MATLAB / matplotlib
inline double nice_num(double x, bool round_it) {
    if (x == 0.0) return 0.0;
    const double exp_v = std::floor(std::log10(std::abs(x)));
    const double frac = std::abs(x) / std::pow(10.0, exp_v);
    double nice;
    if (round_it) {
        if (frac < 1.5)       nice = 1.0;
        else if (frac < 3.0)  nice = 2.0;
        else if (frac < 7.0)  nice = 5.0;
        else                  nice = 10.0;
    } else {
        if (frac <= 1.0)      nice = 1.0;
        else if (frac <= 2.0) nice = 2.0;
        else if (frac <= 5.0) nice = 5.0;
        else                  nice = 10.0;
    }
    return std::copysign(nice * std::pow(10.0, exp_v), x);
}

inline std::vector<double> nice_ticks(double lo, double hi, int target_ticks = 6) {
    if (hi - lo < 1e-14) return { lo };
    const double range = nice_num(hi - lo, false);
    const double d = nice_num(range / std::max(1, target_ticks - 1), true);
    const double graph_min = std::floor(lo / d) * d;
    const double graph_max = std::ceil(hi / d) * d;
    std::vector<double> ticks;
    for (double v = graph_min; v <= graph_max + 0.5 * d; v += d) {
        if (v >= lo - 1e-10 * (hi - lo) && v <= hi + 1e-10 * (hi - lo))
            ticks.push_back(v);
    }
    if (ticks.empty()) ticks.push_back(0.5 * (lo + hi));
    return ticks;
}

// Format tick value: use integer when possible, otherwise compact float
inline std::string fmt_tick(double v) {
    if (std::abs(v) < 1e-12) return "0";
    if (std::abs(v) >= 1.0 && std::abs(v - std::round(v)) < 1e-9)
        return std::format("{}", (long long)std::round(v));
    // Choose precision based on magnitude
    int prec = std::max(0, (int)(2 - std::floor(std::log10(std::abs(v)))));
    prec = std::min(prec, 6);
    std::string s = std::format("{:.{}f}", v, prec);
    // Strip trailing zeros after decimal point
    if (s.find('.') != std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

inline std::string svg_escape(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default: out += c; break;
        }
    }
    return out;
}

struct Rgba8 {
    uint8_t r=0,g=0,b=0,a=255;
};

inline std::string svg_rgb(const Rgba8& c) {
    return std::format("rgb({},{},{})", c.r, c.g, c.b);
}

struct ColorbarLayout {
    float width = 0.0f;
    float height = 0.0f;
    float x_gap = 0.0f;
    float y_offset = 0.0f;
    float label_gap = 0.0f;
    int right_margin = 0;
    int svg_font_px = 0;
    int bitmap_label_scale = 1;
};

inline ColorbarLayout make_colorbar_layout(float plot_h, int scale) {
    const float scale_f = static_cast<float>(std::max(scale, 1));

    ColorbarLayout layout;
    layout.height = std::clamp(plot_h * 0.42f, 180.0f * scale_f, 320.0f * scale_f);
    layout.width = std::clamp(plot_h * 0.055f, 24.0f * scale_f, 48.0f * scale_f);
    layout.x_gap = std::max(18.0f * scale_f, layout.width * 0.8f);
    layout.y_offset = std::max(10.0f * scale_f, layout.width * 0.25f);
    layout.label_gap = std::max(10.0f * scale_f, layout.width * 0.45f);
    layout.right_margin = (int)std::ceil(layout.x_gap + layout.width + std::max(110.0f * scale_f, layout.width * 2.8f));
    layout.svg_font_px = (int)std::lround(std::max(14.0f * scale_f, layout.width * 0.6f));
    layout.bitmap_label_scale = std::max(scale + 1, (int)std::lround(layout.width / 10.0f));
    return layout;
}

inline bool glyph_5x7(char c, uint8_t rows[7]) {
    auto set = [&](uint8_t r0, uint8_t r1, uint8_t r2, uint8_t r3, uint8_t r4, uint8_t r5, uint8_t r6) {
        rows[0] = r0; rows[1] = r1; rows[2] = r2; rows[3] = r3; rows[4] = r4; rows[5] = r5; rows[6] = r6;
    };
    switch (c) {
    case '0': set(0x0E,0x11,0x13,0x15,0x19,0x11,0x0E); return true;
    case '1': set(0x04,0x0C,0x04,0x04,0x04,0x04,0x0E); return true;
    case '2': set(0x0E,0x11,0x01,0x02,0x04,0x08,0x1F); return true;
    case '3': set(0x0E,0x11,0x01,0x06,0x01,0x11,0x0E); return true;
    case '4': set(0x02,0x06,0x0A,0x12,0x1F,0x02,0x02); return true;
    case '5': set(0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E); return true;
    case '6': set(0x06,0x08,0x10,0x1E,0x11,0x11,0x0E); return true;
    case '7': set(0x1F,0x01,0x02,0x04,0x08,0x08,0x08); return true;
    case '8': set(0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E); return true;
    case '9': set(0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C); return true;
    case '.': set(0x00,0x00,0x00,0x00,0x00,0x06,0x06); return true;
    case '-': set(0x00,0x00,0x00,0x1F,0x00,0x00,0x00); return true;
    case 'e':
    case 'E': set(0x00,0x0E,0x11,0x1F,0x10,0x11,0x0E); return true;
    default:
        return false;
    }
}

inline ImVec2 measure_text_5x7(std::string_view text, int px_scale) {
    const int cw = 5 * px_scale;
    const int ch = 7 * px_scale;
    const int sp = 1 * px_scale;
    int w = 0;
    for (char c : text) {
        w += cw;
        w += sp;
        (void)c;
    }
    if (!text.empty()) w -= sp;
    return ImVec2((float)w, (float)ch);
}

inline void svg_draw_text_5x7(std::ostringstream& ss,
                              float x,
                              float y,
                              std::string_view text,
                              const Rgba8& color,
                              int px_scale,
                              int anchor) {
    ImVec2 sz = measure_text_5x7(text, px_scale);
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
                    ss << "  <rect x=\"" << (pen_x + (float)(rx * px_scale))
                       << "\" y=\"" << (y + (float)(ry * px_scale))
                       << "\" width=\"" << px_scale
                       << "\" height=\"" << px_scale
                       << "\" fill=\"" << svg_rgb(color) << "\"/>\n";
                }
            }
        }
        pen_x += (float)(cw + sp);
    }
}

} // namespace

static void draw_boundary_polyline(ImDrawList* dl, const Viewport& vp,
                                  const std::vector<Point2D>& pts, bool closed)
{
    if (!dl || pts.size() < 2) return;

    std::vector<ImVec2> poly;
    poly.reserve(pts.size() + (closed ? 1 : 0));

    for (const auto& p : pts) {
        ImVec2 s = vp.to_screen(glm::dvec2(p.x(), p.y()));
        poly.push_back(s);
    }
    if (closed) poly.push_back(poly.front());

    dl->AddPolyline(poly.data(), (int)poly.size(), IM_COL32(0,255,0,255), false, 2.0f);
}


void CanvasWindow::draw(const CanvasWindowDrawInfo& draw_info) {
    if (!is_draw_info_valid(draw_info)) {
        last_entity_ = nullptr;
        last_mesh_ = nullptr;
        last_pde_ = nullptr;

        ImGui::Begin("Planar Mesh Canvas", nullptr, ImGuiWindowFlags_NoScrollbar);
        draw_export_popup();
        ImGui::End();
        return;
    }

    ImGui::Begin("Planar Mesh Canvas", nullptr, ImGuiWindowFlags_NoScrollbar);

    last_entity_ = draw_info.selected_entity;
    last_mesh_ = draw_info.selected_mesh;
    last_pde_ = draw_info.selected_pde;

    PlanarMathEntity* math_entity = nullptr;
    if (draw_info.selected_entity->is_a<PlanarMathEntity>()) {
        math_entity = static_cast<PlanarMathEntity*>(draw_info.selected_entity);
    }

    PlanarMeshComponent* mesh = draw_info.selected_mesh;
    PlanarMeshBoundaryBase* edited_boundary = mesh->edited_boundary();

    viewport_.screen_pos = ImGui::GetCursorScreenPos();
    ImVec2 available = ImGui::GetContentRegionAvail();
    viewport_.screen_size = ImVec2(std::max(available.x, 50.0f), std::max(available.y, 50.0f));

    state_.size = { viewport_.screen_size.x, viewport_.screen_size.y };
    state_.position = { viewport_.screen_pos.x, viewport_.screen_pos.y };
    
    ImVec2 canvas_end(
        viewport_.screen_pos.x + viewport_.screen_size.x,
        viewport_.screen_pos.y + viewport_.screen_size.y
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(viewport_.screen_pos, canvas_end, IM_COL32(10,10,0,255));
    draw_list->AddRect(viewport_.screen_pos, canvas_end, IM_COL32(255,255,255,255));

    ImGui::SetCursorScreenPos(viewport_.screen_pos);
    ImGui::InvisibleButton("canvas", viewport_.screen_size);
    bool hovered = ImGui::IsItemHovered();

    if (mesh->is_free_hand_enabled()) {
        SmoothStrokeConfig default_config;
        if (auto loop = stroke_tool_.update(viewport_, hovered, draw_list, default_config)) {
            PlanarMeshOuterBoundary* outer_boundary = mesh->outer_boundary();
            if (outer_boundary->points().empty() || outer_boundary->input_type() != BoundaryInputType::SmoothStroke) {
                outer_boundary->set_input_type(BoundaryInputType::SmoothStroke);
                outer_boundary->set_smooth_stroke_config(default_config);
                outer_boundary->set_points(*loop);
            } else {
                mesh->add_inner_boundary(*loop);
            }
        }
    } else if (edited_boundary) {
        if (edited_boundary->input_type() == BoundaryInputType::SmoothStroke) {
            if (auto loop = stroke_tool_.update(viewport_, hovered, draw_list, edited_boundary->smooth_stroke_config())) {
                edited_boundary->set_points(*loop);
            }
        }
    }

    ImGuiIO& io = ImGui::GetIO();

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (io.KeyAlt) {
            start_rubber_band_selection(glm::vec2(io.MousePos.x, io.MousePos.y));
            selection_mode_ = SelectionMode::RubberBand;
        } else {
            on_click(mesh->edited_boundary());
        }
    }

    if (is_rubber_band_active_ && hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        update_rubber_band_selection(glm::vec2(io.MousePos.x, io.MousePos.y));
    }

    if (is_rubber_band_active_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        finish_rubber_band_selection(draw_info);
    }

    if (is_rubber_band_active_ && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        cancel_rubber_band_selection();
    }

    if (!is_draw_info_valid(draw_info)) {
        inspector_.clear();
        return;
    }

    const DelaunayTriangulationResult& R = draw_info.selected_mesh->triangulation_result();

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) on_right_click(draw_info);
    viewport_.handle_input();

    draw_solution(draw_info);

    if (math_entity) {
        for (MeshComponent* mesh_component : math_entity->mesh_components()) {
            PlanarMeshComponent* planar_mesh_component = static_cast<PlanarMeshComponent*>(mesh_component);
            draw_mesh(planar_mesh_component);
        }
    }

    inspector_.draw(R);

    draw_selection_overlay(draw_info);
    
    draw_rubber_band();

    BoundaryCondition* edited_bc = draw_info.selected_mesh->edited_boundary_condition();
    if (edited_bc) {
        ImGui::SetCursorPos(ImVec2(10, ImGui::GetCursorPosY()));
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 
                          "BC Edit Mode: Alt+Drag to select edges, Ctrl+Alt+Drag to add");
    }

    draw_export_popup();

    ImGui::End();
}

void CanvasWindow::on_click(PlanarMeshBoundaryBase* selected_boundary) {
    if (!selected_boundary) {
        return;
    }

    if (selected_boundary->input_type() != BoundaryInputType::Polygon) {
        return;
    }

    if (!selected_boundary->is_editing_enabled()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    if (!io.KeyShift) {
        return;
    }

    glm::dvec2 world_pos = viewport_.to_world(io.MousePos);

    selected_boundary->add_point(world_pos);
}


void CanvasWindow::on_right_click(const CanvasWindowDrawInfo& draw_info) {
    if (!is_draw_info_valid(draw_info)) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    glm::dvec2 world_pos = viewport_.to_world(io.MousePos);

    const auto& triangulation_result = draw_info.selected_mesh->triangulation_result();

    // Convert pixel-pick radii to world units via viewport zoom
    double z = std::max(1e-6, (double)viewport_.zoom);
    const double vr_world = vertex_pick_radius_px_ / z;
    const double er_world = edge_pick_radius_px_   / z;
    const double er_world_sq = er_world * er_world;

    if (BoundaryCondition* edited_bc = draw_info.selected_mesh->edited_boundary_condition()) {
        int v = pick_vertex(triangulation_result, world_pos, vr_world);

        if (v >= 0 && gui::bc::is_boundary_vertex(triangulation_result, v)) {
            if (edited_bc->start_point() < 0) {
                edited_bc->set_start_point(v);
            } else {
                edited_bc->set_end_point(v);
            }

            return;
        }

        int e = pick_edge(triangulation_result, world_pos, er_world_sq);
        if (e >= 0 && (size_t)e < triangulation_result.edges.size() && triangulation_result.edges[e].on_boundary) {
            const auto& E = triangulation_result.edges[e];
            const auto& A = triangulation_result.points[E.a];
            const auto& B = triangulation_result.points[E.b];
            double dA2 = (world_pos.x - A.x())*(world_pos.x - A.x()) + (world_pos.y - A.y())*(world_pos.y - A.y());
            double dB2 = (world_pos.x - B.x())*(world_pos.x - B.x()) + (world_pos.y - B.y())*(world_pos.y - B.y());
            int v_edge = (dA2 <= dB2) ? E.a : E.b;

            if (edited_bc->start_point() < 0) {
                edited_bc->set_start_point(v_edge);
            } else {
                edited_bc->set_end_point(v_edge);
            }
        }        
    }

    const ImVec2 mouse_screen = io.MousePos;

    inspector_.settings.vertex_pick_radius_px = vertex_pick_radius_px_;
    inspector_.settings.edge_pick_radius_px   = edge_pick_radius_px_;
    inspector_.settings.boundary_edges_only   = false;
    inspector_.settings.use_tooltip           = true;

    inspector_.on_right_click(triangulation_result, world_pos, mouse_screen, viewport_.zoom);

    if (draw_info.on_inspector_pick) {
        draw_info.on_inspector_pick(draw_info.selected_mesh, inspector_.selection(), world_pos);
    }
}

void CanvasWindow::draw_solution(const CanvasWindowDrawInfo& draw_info) {
    if (!draw_info.selected_pde) {
        return;
    }

    auto [global_u_min, global_u_max] = draw_info.selected_pde->get_global_bounds();

    if (global_u_min > global_u_max) return;

    ImVec2 cb_pos(state_.position.x + 10, state_.position.y + 10);
    ImVec2 cb_size(20, 120);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto color_for_u = [global_u_min, global_u_max](double u)->ImU32 {
        double t = (global_u_max > global_u_min)
            ? (u - global_u_min) / (global_u_max - global_u_min)
            : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        int r = int(255 * t), g = 50, b = int(255 * (1.0 - t));
        return IM_COL32(r, g, b, 200);
    };

    PlanarMathEntity* math_entity = nullptr;
    if (draw_info.selected_entity->is_a<PlanarMathEntity>()) {
        math_entity = static_cast<PlanarMathEntity*>(draw_info.selected_entity);
    }

    if (math_entity) {
        for (MeshComponent* mesh_component : math_entity->mesh_components()) {
            PlanarMeshComponent* submesh = static_cast<PlanarMeshComponent*>(mesh_component);
            const DifferentialEquationSolution& solution = draw_info.selected_pde->solution(submesh);

            if (!solution.is_ready()) {
                continue;
            }

            const DelaunayTriangulationResult& result = submesh->triangulation_result();

            if (solution.solution_u.size() == result.points.size()) {
                for (const auto& tri : result.triangles) {
                    if (!tri.valid) continue;
                    
                    ImVec2 p1 = viewport_.to_screen({result.points[tri.v[0]].x(), result.points[tri.v[0]].y()});
                    ImVec2 p2 = viewport_.to_screen({result.points[tri.v[1]].x(), result.points[tri.v[1]].y()});
                    ImVec2 p3 = viewport_.to_screen({result.points[tri.v[2]].x(), result.points[tri.v[2]].y()});

                    double u0 = solution.solution_u[tri.v[0]];
                    double u1 = solution.solution_u[tri.v[1]];
                    double u2 = solution.solution_u[tri.v[2]];
                    double uavg = (u0 + u1 + u2) / 3.0;

                    draw_list->AddTriangleFilled(p1, p2, p3, color_for_u(uavg));
                }
            }
        }
    } else {
        const DifferentialEquationSolution& solution = draw_info.selected_pde->solution();
        if (!solution.is_ready()) {
            return;
        }

        const DelaunayTriangulationResult& result = draw_info.selected_mesh->triangulation_result();

        if (solution.solution_u.size() == result.points.size()) {
            for (const auto& tri : result.triangles) {
                if (!tri.valid) continue;
                
                ImVec2 p1 = viewport_.to_screen({result.points[tri.v[0]].x(), result.points[tri.v[0]].y()});
                ImVec2 p2 = viewport_.to_screen({result.points[tri.v[1]].x(), result.points[tri.v[1]].y()});
                ImVec2 p3 = viewport_.to_screen({result.points[tri.v[2]].x(), result.points[tri.v[2]].y()});

                double u0 = solution.solution_u[tri.v[0]];
                double u1 = solution.solution_u[tri.v[1]];
                double u2 = solution.solution_u[tri.v[2]];
                double uavg = (u0 + u1 + u2) / 3.0;

                draw_list->AddTriangleFilled(p1, p2, p3, color_for_u(uavg));
            }
        }
    }

    // Draw colorbar with global min/max
    int segments = 40;
    for (int i = 0; i < segments; ++i) {
        double t0 = double(i) / segments;
        double t1 = double(i + 1) / segments;
        double u0 = global_u_min + t0 * (global_u_max - global_u_min);
        double u1 = global_u_min + t1 * (global_u_max - global_u_min);

        float y0 = cb_pos.y + (1.0f - (float)t0) * cb_size.y;
        float y1 = cb_pos.y + (1.0f - (float)t1) * cb_size.y;

        draw_list->AddRectFilled(
            ImVec2(cb_pos.x,     y0),
            ImVec2(cb_pos.x + cb_size.x, y1),
            color_for_u(0.5 * (u0 + u1))
        );
    }
    draw_list->AddRect(cb_pos, ImVec2(cb_pos.x + cb_size.x, cb_pos.y + cb_size.y),
                IM_COL32(255,255,255,255));

    std::string u_min_text = "u_min=" + fmt_no_plus(global_u_min);
    std::string u_max_text = "u_max=" + fmt_no_plus(global_u_max);

    // Min / max labels
    ImGui::GetForegroundDrawList()->AddText(
        ImVec2(cb_pos.x + cb_size.x + 5, cb_pos.y - ImGui::GetTextLineHeight()*0.5f),
        IM_COL32(255,255,255,255),
        u_max_text.c_str()
    );
    ImGui::GetForegroundDrawList()->AddText(
        ImVec2(cb_pos.x + cb_size.x + 5, cb_pos.y + cb_size.y - ImGui::GetTextLineHeight()*0.5f),
        IM_COL32(255,255,255,255),
        u_min_text.c_str()
    );
}

void CanvasWindow::draw_export_popup() {
    constexpr const char* kPopupName = "Export Canvas";

    if (export_popup_requested_) {
        ImGui::OpenPopup(kPopupName);
        export_popup_requested_ = false;
        export_popup_open_ = true;

        if (export_path_.empty()) {
            export_path_ = (export_settings_.format_index == 0) ? "canvas.png" : "canvas.svg";
        }
    }

    if (!export_popup_open_) {
        return;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal(kPopupName, &export_popup_open_, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    const bool has_mesh = last_mesh_ != nullptr;
    const bool has_pde = last_pde_ != nullptr;
    bool has_bounds = false;
    double u_min = 0.0, u_max = 0.0;
    if (has_pde) {
        auto bounds = last_pde_->get_global_bounds();
        u_min = bounds.first;
        u_max = bounds.second;
        has_bounds = (u_min <= u_max);
    }

    ImGui::TextUnformatted("Export current canvas view for publications.");
    ImGui::Separator();

    {
        const char* formats[] = { "PNG (raster)", "SVG (vector)" };
        int prev_format = export_settings_.format_index;
        if (ImGui::Combo("Format", &export_settings_.format_index, formats, IM_ARRAYSIZE(formats))) {
            auto ends_with = [](const std::string& s, const char* suf) {
                size_t n = s.size(), m = std::strlen(suf);
                if (n < m) return false;
                return s.compare(n - m, m, suf) == 0;
            };

            if (prev_format != export_settings_.format_index) {
                if (ends_with(export_path_, ".png") && export_settings_.format_index == 1) {
                    export_path_.resize(export_path_.size() - 4);
                    export_path_ += ".svg";
                } else if (ends_with(export_path_, ".svg") && export_settings_.format_index == 0) {
                    export_path_.resize(export_path_.size() - 4);
                    export_path_ += ".png";
                } else if (export_path_.empty()) {
                    export_path_ = (export_settings_.format_index == 0) ? "canvas.png" : "canvas.svg";
                }
            }
        }
    }

    {
        const char* themes[] = { "Dark", "Light" };
        int theme_idx = (export_settings_.theme == ExportSettings::Theme::Dark) ? 0 : 1;
        if (ImGui::Combo("Theme", &theme_idx, themes, IM_ARRAYSIZE(themes))) {
            export_settings_.theme = (theme_idx == 0) ? ExportSettings::Theme::Dark : ExportSettings::Theme::Light;
        }
    }

    ImGui::SliderInt("Scale", &export_settings_.scale_factor, 1, 6, "x%d");
    ImGui::Checkbox("Axes + ticks", &export_settings_.include_axes);

    ImGui::BeginDisabled(!has_pde);
    ImGui::Checkbox("Solution fill", &export_settings_.include_solution);
    ImGui::EndDisabled();

    ImGui::Checkbox("Mesh", &export_settings_.include_mesh);
    ImGui::Checkbox("Points", &export_settings_.include_points);
    ImGui::Checkbox("Boundary Conditions", &export_settings_.include_boundary_conditions);
    
    ImGui::BeginDisabled(!export_settings_.include_boundary_conditions);
    ImGui::Checkbox("BC Legend", &export_settings_.include_bc_legend);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!(has_pde && has_bounds));
    ImGui::Checkbox("Colorbar", &export_settings_.include_colorbar);
    ImGui::EndDisabled();

    if (!has_pde || !has_bounds) {
        ImGui::TextDisabled("Colorbar: unavailable (no ready PDE bounds)");
    } else {
        ImGui::TextDisabled("u range: [%s, %s]", fmt_no_plus(u_min).c_str(), fmt_no_plus(u_max).c_str());
    }

    ImGui::Separator();

    ImGui::InputText("Path", &export_path_);
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        const char* ext = (export_settings_.format_index == 0) ? "png" : "svg";
        std::string p = FileSystem::save_file_dialog("Export Canvas", { ext });
        if (!p.empty()) {
            export_path_ = p + "." + ext;
        }
    }

    if (!has_mesh) {
        ImGui::TextDisabled("Nothing to export: no selected mesh.");
    }

    ImGui::BeginDisabled(!has_mesh || export_path_.empty());
    if (ImGui::Button("Export", ImVec2(120, 0))) {
        bool ok = (export_settings_.format_index == 0) 
            ? export_png(export_path_) 
            : export_svg(export_path_);
        if (ok) {
            export_popup_open_ = false;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
        export_popup_open_ = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

bool CanvasWindow::export_svg(const std::string& absolute_path) const {
    if (!last_mesh_) {
        return false;
    }

    const auto& R = last_mesh_->triangulation_result();
    if (state_.size.x <= 1.0f || state_.size.y <= 1.0f || R.points.empty()) {
        return false;
    }

    // Compute tight bounding box from mesh geometry (not viewport)
    double mx_min = R.points[0].x(), mx_max = mx_min;
    double my_min = R.points[0].y(), my_max = my_min;
    for (const auto& pt : R.points) {
        mx_min = std::min(mx_min, pt.x()); mx_max = std::max(mx_max, pt.x());
        my_min = std::min(my_min, pt.y()); my_max = std::max(my_max, pt.y());
    }
    // Add 3% padding around the mesh
    const double pad_x = std::max(1.0, (mx_max - mx_min) * 0.03);
    const double pad_y = std::max(1.0, (my_max - my_min) * 0.03);
    const double wx_min = mx_min - pad_x;
    const double wx_max = mx_max + pad_x;
    const double wy_min = my_min - pad_y;
    const double wy_max = my_max + pad_y;

    const int scale = std::max(1, export_settings_.scale_factor);
    
    const bool dark = export_settings_.theme == ExportSettings::Theme::Dark;
    const Rgba8 bg = dark ? Rgba8{30, 30, 30, 255} : Rgba8{255, 255, 255, 255};
    const Rgba8 fg = dark ? Rgba8{220, 220, 220, 255} : Rgba8{30, 30, 30, 255};
    const Rgba8 grid = dark ? Rgba8{80, 80, 80, 130} : Rgba8{200, 200, 200, 180};
    const Rgba8 plot_bg = dark ? Rgba8{15, 15, 15, 255} : Rgba8{255, 255, 255, 255};

    int margin_l = 80 * scale;
    int margin_b = 60 * scale;
    int margin_t = 30 * scale;
    int margin_r = 30 * scale;

    bool have_bounds = false;
    double u_min = 0.0, u_max = 0.0;
    if (last_pde_) {
        auto bounds = last_pde_->get_global_bounds();
        u_min = bounds.first;
        u_max = bounds.second;
        have_bounds = (u_min <= u_max);
    }

    const bool draw_colorbar = export_settings_.include_colorbar && have_bounds && export_settings_.include_solution;
    const int estimated_plot_h = std::max(64, (int)std::lround(state_.size.y * (float)scale) - margin_t - margin_b);
    ColorbarLayout colorbar_layout;
    if (draw_colorbar) {
        colorbar_layout = make_colorbar_layout((float)estimated_plot_h, scale);
        margin_r = std::max(margin_r, colorbar_layout.right_margin);
    }

    const double world_w = std::max(1e-12, wx_max - wx_min);
    const double world_h = std::max(1e-12, wy_max - wy_min);
    const double world_aspect = world_w / world_h;
    
    int base_plot_w = (int)std::lround(state_.size.x * (float)scale) - margin_l - margin_r;
    int base_plot_h = (int)std::lround(state_.size.y * (float)scale) - margin_t - margin_b;
    base_plot_w = std::max(64, base_plot_w);
    base_plot_h = std::max(64, base_plot_h);
    
    const double screen_aspect = (double)base_plot_w / (double)base_plot_h;
    int plot_w, plot_h;
    if (world_aspect > screen_aspect) {
        plot_w = base_plot_w;
        plot_h = (int)std::lround(plot_w / world_aspect);
    } else {
        plot_h = base_plot_h;
        plot_w = (int)std::lround(plot_h * world_aspect);
    }
    
    // Center the plot region – use base dimensions for canvas so axes are never clipped
    const int W = base_plot_w + margin_l + margin_r;
    const int H = base_plot_h + margin_t + margin_b;
    
    const int plot_x0 = margin_l + (base_plot_w - plot_w) / 2;
    const int plot_y0 = margin_t + (base_plot_h - plot_h) / 2;

    if (draw_colorbar) {
        colorbar_layout = make_colorbar_layout((float)plot_h, scale);
    }

    auto to_px = [&](double wx, double wy) -> ImVec2 {
        const double tx = (wx - wx_min) / world_w;
        const double ty = (wy_max - wy) / world_h; // y-down
        return ImVec2(
            (float)(plot_x0 + tx * plot_w),
            (float)(plot_y0 + ty * plot_h)
        );
    };

    // Professional colormap (Parula-inspired: dark blue -> teal -> yellow)
    auto color_for_u = [&](double u) -> Rgba8 {
        if (!have_bounds) return Rgba8{128, 128, 128, 220};
        double t = (u_max > u_min) ? (u - u_min) / (u_max - u_min) : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        // 5-stop Parula-like: dark blue -> blue -> teal -> yellow-green -> yellow
        struct Stop { double t; uint8_t r, g, b; };
        constexpr Stop stops[] = {
            {0.00,  53,  42, 135},  // dark indigo
            {0.25,  15, 111, 198},  // blue
            {0.50,   3, 167, 133},  // teal
            {0.75, 144, 205,  57},  // lime
            {1.00, 249, 251,  21},  // yellow
        };
        int seg = 0;
        for (int i = 0; i < 4; ++i) { if (t >= stops[i+1].t) seg = i+1; else break; }
        seg = std::min(seg, 3);
        const auto& s0 = stops[seg];
        const auto& s1 = stops[seg+1];
        double lt = (t - s0.t) / (s1.t - s0.t);
        lt = std::clamp(lt, 0.0, 1.0);
        uint8_t r = (uint8_t)std::lround(s0.r + lt * (s1.r - s0.r));
        uint8_t g = (uint8_t)std::lround(s0.g + lt * (s1.g - s0.g));
        uint8_t b = (uint8_t)std::lround(s0.b + lt * (s1.b - s0.b));
        return Rgba8{r, g, b, 230};
    };

    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << W << "\" height=\"" << H
       << "\" viewBox=\"0 0 " << W << " " << H << "\">\n";

    // Background
    ss << "  <rect x=\"0\" y=\"0\" width=\"" << W << "\" height=\"" << H
       << "\" fill=\"" << svg_rgb(bg) << "\"/>\n";

    // Clip plot area
    ss << "  <defs>\n";
    ss << "    <clipPath id=\"plotClip\"><rect x=\"" << plot_x0 << "\" y=\"" << plot_y0
       << "\" width=\"" << plot_w << "\" height=\"" << plot_h << "\"/></clipPath>\n";
    if (draw_colorbar) {
        ss << "    <linearGradient id=\"cbGrad\" x1=\"0\" y1=\"1\" x2=\"0\" y2=\"0\">\n";
        constexpr int kColorbarStops = 16;
        for (int i = 0; i <= kColorbarStops; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(kColorbarStops);
            const double u = u_min + t * (u_max - u_min);
            const Rgba8 c = color_for_u(u);
            ss << "      <stop offset=\"" << (t * 100.0) << "%\" stop-color=\"" << svg_rgb(c)
               << "\" stop-opacity=\"" << (static_cast<double>(c.a) / 255.0) << "\"/>\n";
        }
        ss << "    </linearGradient>\n";
    }
    ss << "  </defs>\n";

    // Plot frame
    if (export_settings_.include_axes) {
        ss << "  <rect x=\"" << plot_x0 << "\" y=\"" << plot_y0 << "\" width=\"" << plot_w
           << "\" height=\"" << plot_h << "\" fill=\"none\" stroke=\"" << svg_rgb(fg)
           << "\" stroke-width=\"" << (2 * scale) << "\"/>\n";
    }

    ss << "  <g clip-path=\"url(#plotClip)\">\n";

    if (export_settings_.include_solution && last_pde_ && have_bounds) {
        const DifferentialEquationSolution& sol = last_pde_->solution(last_mesh_);
        if (sol.is_ready() && sol.solution_u.size() == R.points.size()) {
            for (const auto& tri : R.triangles) {
                if (!tri.valid) continue;
                const auto& A = R.points[tri.v[0]];
                const auto& B = R.points[tri.v[1]];
                const auto& C = R.points[tri.v[2]];
                const double uavg = (sol.solution_u[tri.v[0]] + sol.solution_u[tri.v[1]] + sol.solution_u[tri.v[2]]) / 3.0;
                const Rgba8 col = color_for_u(uavg);
                const ImVec2 pA = to_px(A.x(), A.y());
                const ImVec2 pB = to_px(B.x(), B.y());
                const ImVec2 pC = to_px(C.x(), C.y());
                ss << "    <polygon points=\"" << pA.x << "," << pA.y << " " << pB.x << "," << pB.y << " " << pC.x
                   << "," << pC.y << "\" fill=\"" << svg_rgb(col) << "\" fill-opacity=\"0.784\" stroke=\"none\"/>\n";
            }
        }
    }

    auto bc_color = [&](BoundaryConditionType::Type t) -> Rgba8 {
        switch (t) {
        case BoundaryConditionType::Type::Dirichlet: return dark ? Rgba8{255, 120, 200, 255} : Rgba8{180, 0, 140, 255};
        case BoundaryConditionType::Type::Neumann:   return dark ? Rgba8{80, 220, 255, 255} : Rgba8{0, 130, 180, 255};
        case BoundaryConditionType::Type::Robin:     return dark ? Rgba8{255, 210, 80, 255} : Rgba8{200, 130, 0, 255};
        default:                               return Rgba8{120, 120, 120, 255};
        }
    };


    // Mesh edges
    if (export_settings_.include_mesh) {
        const float thin = std::max(0.6f, (scale > 2) ? scale * 0.45f : 1.0f);
        const float thick = std::max(1.2f, scale * 1.0f);
        
        const bool solution_visible = export_settings_.include_solution && last_pde_ && have_bounds;
        const Rgba8 edge_col = solution_visible 
                               ? (dark ? Rgba8{200, 200, 200, 160} : Rgba8{80, 80, 80, 120})
                               : (dark ? Rgba8{100, 100, 100, 200} : Rgba8{50, 50, 50, 255});
        const Rgba8 boundary_col = dark ? Rgba8{140, 200, 140, 240} : Rgba8{30, 30, 30, 255};
        
        for (const EdgeInfo& e : R.edges) {
            if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;
            const auto& A = R.points[e.a];
            const auto& B = R.points[e.b];
            const ImVec2 pA = to_px(A.x(), A.y());
            const ImVec2 pB = to_px(B.x(), B.y());
            const Rgba8 c = e.on_boundary ? boundary_col : edge_col;
            const float w = e.on_boundary ? thick : thin;
            const float opacity = (float)c.a / 255.0f;
            ss << "    <line x1=\"" << pA.x << "\" y1=\"" << pA.y << "\" x2=\"" << pB.x << "\" y2=\"" << pB.y
               << "\" stroke=\"" << svg_rgb(c) << "\" stroke-opacity=\"" << opacity << "\" stroke-width=\"" << w << "\" stroke-linecap=\"round\"/>\n";
        }
    }

    if (export_settings_.include_boundary_conditions && last_mesh_) {
        const float bc_thickness = std::max(1.5f, scale * 1.2f);
        for (BoundaryCondition* bc : last_mesh_->boundary_conditions()) {
            const Rgba8 color = bc_color(bc->type().value);
            for (int eid : bc->edge_ids()) {
                if ((size_t)eid >= R.edges.size()) continue;
                const EdgeInfo& e = R.edges[eid];
                if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;
                const auto& A = R.points[e.a];
                const auto& B = R.points[e.b];
                const ImVec2 pA = to_px(A.x(), A.y());
                const ImVec2 pB = to_px(B.x(), B.y());
                ss << "    <line x1=\"" << pA.x << "\" y1=\"" << pA.y << "\" x2=\"" << pB.x << "\" y2=\"" << pB.y
                   << "\" stroke=\"" << svg_rgb(color) << "\" stroke-width=\"" << bc_thickness << "\" stroke-linecap=\"round\"/>\n";
            }
        }
    }

    if (export_settings_.include_points) {
        const float r = std::max(1.0f, point_radius_ * (float)scale * 0.85f);
        const Rgba8 in_col = dark ? Rgba8{230, 120, 120, 230} : Rgba8{180, 50, 50, 240};
        const Rgba8 bd_col = dark ? Rgba8{100, 200, 120, 230} : Rgba8{40, 120, 50, 240};
        for (const Point2D& pt : R.points) {
            const ImVec2 p = to_px(pt.x(), pt.y());
            const Rgba8 c = pt.on_boundary ? bd_col : in_col;
            ss << "    <circle cx=\"" << p.x << "\" cy=\"" << p.y << "\" r=\"" << r
               << "\" fill=\"" << svg_rgb(c) << "\" stroke=\"none\"/>\n";
        }
    }

    ss << "  </g>\n";

    if (export_settings_.include_axes) {
        const int tick_len = 6 * scale;
        const int label_scale = std::max(1, scale);

        // X-axis ticks (rounded via nice_ticks)
        const auto x_ticks = nice_ticks(wx_min, wx_max, 6);
        for (double wx : x_ticks) {
            double t = (wx - wx_min) / (wx_max - wx_min);
            float x = (float)(plot_x0 + t * plot_w);
            float y0 = (float)(plot_y0 + plot_h);
            float y1 = y0 + tick_len;
            ss << "  <line x1=\"" << x << "\" y1=\"" << y0 << "\" x2=\"" << x << "\" y2=\"" << y1
               << "\" stroke=\"" << svg_rgb(fg) << "\" stroke-width=\"" << (1 * scale) << "\"/>\n";
                svg_draw_text_5x7(ss, x, y1 + (float)(4 * scale), fmt_tick(wx), fg, label_scale, 1);
        }

        // Y-axis ticks (rounded via nice_ticks)
        const auto y_ticks = nice_ticks(wy_min, wy_max, 6);
        for (double wy : y_ticks) {
            double t = (wy - wy_min) / (wy_max - wy_min);
            float y = (float)(plot_y0 + (1.0 - t) * plot_h);
            float x0 = (float)plot_x0;
            float x1 = x0 - tick_len;
            ss << "  <line x1=\"" << x0 << "\" y1=\"" << y << "\" x2=\"" << x1 << "\" y2=\"" << y
               << "\" stroke=\"" << svg_rgb(fg) << "\" stroke-width=\"" << (1 * scale) << "\"/>\n";
            svg_draw_text_5x7(ss, x1 - (float)(4 * scale), y - (float)std::lround(3.5f * label_scale), fmt_tick(wy), fg, label_scale, 2);
        }
    }

    if (draw_colorbar) {
        const int cb_w = (int)std::lround(colorbar_layout.width);
        const int cb_h = (int)std::lround(std::min<float>(plot_h, colorbar_layout.height));
        const int cb_x = plot_x0 + plot_w + (int)std::lround(colorbar_layout.x_gap);
        const int cb_y = plot_y0 + (int)std::lround(colorbar_layout.y_offset);

        ss << "  <rect x=\"" << cb_x << "\" y=\"" << cb_y << "\" width=\"" << cb_w << "\" height=\"" << cb_h
           << "\" fill=\"url(#cbGrad)\" stroke=\"" << svg_rgb(fg) << "\" stroke-width=\"" << (1 * scale) << "\"/>\n";

        const int label_scale = colorbar_layout.bitmap_label_scale;
        const std::string u_max_label = fmt_tick(u_max);
        const std::string u_min_label = fmt_tick(u_min);
        const ImVec2 u_min_label_size = measure_text_5x7(u_min_label, label_scale);
        svg_draw_text_5x7(ss,
                          (float)(cb_x + cb_w + colorbar_layout.label_gap),
                          (float)cb_y,
                          u_max_label,
                          fg,
                          label_scale,
                          0);
        svg_draw_text_5x7(ss,
                          (float)(cb_x + cb_w + colorbar_layout.label_gap),
                          (float)(cb_y + cb_h - std::lround(u_min_label_size.y)),
                          u_min_label,
                          fg,
                          label_scale,
                          0);
    }

    if (export_settings_.include_bc_legend && export_settings_.include_boundary_conditions && last_mesh_) {
        auto bc_name = [](BoundaryConditionType::Type t) -> const char* {
            switch (t) {
            case BoundaryConditionType::Type::Dirichlet: return "Dirichlet";
            case BoundaryConditionType::Type::Neumann:   return "Neumann";
            case BoundaryConditionType::Type::Robin:     return "Robin";
            default:                               return "Unknown";
            }
        };

        const int font_px = 11 * scale;
        const int legend_x = 10 * scale;
        const int legend_y = plot_y0 + plot_h - 10 * scale;
        const int line_h = 18 * scale;
        const int line_w = 25 * scale;
        
        int y_offset = 0;
        std::vector<BoundaryConditionType> bc_types;
        for (BoundaryCondition* bc : last_mesh_->boundary_conditions()) {
            bool found = false;
            for (BoundaryConditionType existing : bc_types) {
                if (existing == bc->type()) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                bc_types.push_back(bc->type());
            }
        }
        
        for (BoundaryConditionType bc_type : bc_types) {
            const Rgba8 color = bc_color(bc_type.value);
            const float y = (float)(legend_y - y_offset);
            const float x1 = (float)legend_x;
            const float x2 = (float)(legend_x + line_w);
            
            ss << "  <line x1=\"" << x1 << "\" y1=\"" << y << "\" x2=\"" << x2 << "\" y2=\"" << y
               << "\" stroke=\"" << svg_rgb(color) << "\" stroke-width=\"" << (2.5f * scale)
               << "\" stroke-linecap=\"round\"/>\n";
                ss << "  <text x=\"" << (x2 + 6 * scale) << "\" y=\"" << y
                    << "\" fill=\"" << svg_rgb(fg) << "\" font-size=\"" << font_px
                    << "\" font-family=\"Helvetica, Arial, sans-serif\" text-anchor=\"start\""
                    << " dominant-baseline=\"middle\">" << svg_escape(bc_name(bc_type.value)) << "</text>\n";
            
            y_offset += line_h;
        }
    }

    ss << "</svg>\n";

    FileSystem::write(absolute_path, ss.str());
    return true;
}

void CanvasWindow::draw_mesh(PlanarMeshComponent* mesh) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto in_bounds = [](int idx, size_t n) -> bool {
        return idx >= 0 && (size_t)idx < n;
    };

    const DelaunayTriangulationResult& R = mesh->triangulation_result();

    auto screen_pt = [&](int vid) -> ImVec2 {
        const auto& p = R.points[vid];
        return viewport_.to_screen({p.x(), p.y()});
    };

    auto bc_color = [](BoundaryConditionType::Type t) -> ImU32 {
        switch (t) {
        case BoundaryConditionType::Type::Dirichlet: return IM_COL32(255, 0,   0,   255);
        case BoundaryConditionType::Type::Neumann:   return IM_COL32(0,   0,   255, 255);
        case BoundaryConditionType::Type::Robin:     return IM_COL32(255, 128, 0,   255);
        default:                               return IM_COL32(100, 100, 100, 255);
        }
    };

    auto draw_edge_segment = [&](const EdgeInfo& e, ImU32 color, float thickness) {
        if (!in_bounds(e.a, R.points.size()) || !in_bounds(e.b, R.points.size())) {
            return;
        }
        draw_list->AddLine(screen_pt(e.a), screen_pt(e.b), color, thickness);
    };

    for (const EdgeInfo& e : R.edges) {
        ImU32 color = IM_COL32(100, 100, 100, 255);
        float thickness = 1.0f;

        if (e.on_boundary) {
            color = IM_COL32(0, 255, 0, 255);
            thickness = 2.0f;
        }

        draw_edge_segment(e, color, thickness);
    }

    constexpr float kBcThickness = 3.0f;
    constexpr ImU32 kEditColor = IM_COL32(255, 255, 0, 255);
    constexpr float kEditThickness = 4.0f;

    for (BoundaryCondition* bc : mesh->boundary_conditions()) {
        bool is_edited_bc = bc == mesh->edited_boundary_condition();
        const ImU32 color = bc_color(bc->type().value);

        for (int eid : bc->edge_ids()) {
            if (!in_bounds(eid, R.edges.size())) {
                continue;
            }

            if (is_edited_bc) {
                draw_edge_segment(R.edges[eid], kEditColor, kEditThickness);
            } else {
                draw_edge_segment(R.edges[eid], color, kBcThickness);
            }
        }
    }

    for (const Point2D& point : R.points) {
        ImVec2 pos = viewport_.to_screen({point.x(), point.y()});
        ImU32 color = point.on_boundary ? IM_COL32(100,255,100,255) : IM_COL32(255,100,100,255);
        draw_list->AddCircleFilled(pos, point_radius_, color);
    }
}

int CanvasWindow::pick_vertex(const DelaunayTriangulationResult& R, const glm::dvec2& world_pos, double r_px) {
    double x = world_pos.x;
    double y = world_pos.y;

    const double r2 = r_px*r_px;
    int best = -1; double best_d2 = 1e300;
    for (size_t i=0;i<R.points.size();++i) {
        double dx = x - R.points[i].x(), dy = y - R.points[i].y();
        double d2 = dx*dx + dy*dy;
        if (d2 < r2 && d2 < best_d2) { best_d2 = d2; best = (int)i; }
    }
    return best;
}

int CanvasWindow::pick_edge(const DelaunayTriangulationResult& R, const glm::dvec2& world_pos, double max_d2)
{
    double x = world_pos.x;
    double y = world_pos.y;

    double best = 1e300; int best_e = -1;
    for (size_t i=0;i<R.edges.size();++i) {
        const auto& e = R.edges[i];
        if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;
        const auto& P = R.points[e.a]; const auto& Q = R.points[e.b];
        double A=x-P.x(), B=y-P.y(), C=Q.x()-P.x(), D=Q.y()-P.y();
        double len2 = C*C + D*D; if (len2 < 1e-12) continue;
        double t = (A*C + B*D) / len2; t = std::clamp(t, 0.0, 1.0);
        double px = P.x() + t*C, py = P.y() + t*D;
        double dx = x - px, dy = y - py; double d2 = dx*dx + dy*dy;
        if (d2 < best) { best = d2; best_e = (int)i; }
    }
    return (best <= max_d2) ? best_e : -1;
}

void CanvasWindow::request_triangulation(const PlanarTriangulationSessionConfig& config) {
    FEM_TRIGGER_EVENT(PlanarTriangulationRequest, config);
}

bool CanvasWindow::is_draw_info_valid(const CanvasWindowDrawInfo& draw_info) const {
    return draw_info.selected_entity && draw_info.selected_mesh && draw_info.triangulation_session_config && draw_info.mesh_generator_state;
}

bool CanvasWindow::export_png(const std::string& absolute_path) const {
    if (!last_mesh_) {
        return false;
    }

    const auto& R = last_mesh_->triangulation_result();
    if (state_.size.x <= 1.0f || state_.size.y <= 1.0f || R.points.empty()) {
        return false;
    }

    // Compute tight bounding box from mesh geometry (not viewport)
    double mx_min = R.points[0].x(), mx_max = mx_min;
    double my_min = R.points[0].y(), my_max = my_min;
    for (const auto& pt : R.points) {
        mx_min = std::min(mx_min, pt.x()); mx_max = std::max(mx_max, pt.x());
        my_min = std::min(my_min, pt.y()); my_max = std::max(my_max, pt.y());
    }
    const double pad_x = std::max(1.0, (mx_max - mx_min) * 0.03);
    const double pad_y = std::max(1.0, (my_max - my_min) * 0.03);
    const double wx_min = mx_min - pad_x;
    const double wx_max = mx_max + pad_x;
    const double wy_min = my_min - pad_y;
    const double wy_max = my_max + pad_y;

    const int scale = std::max(1, export_settings_.scale_factor);
    
    const bool dark = export_settings_.theme == ExportSettings::Theme::Dark;
    const auto rgba8 = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) -> uint32_t {
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    };

    const uint32_t bg = dark ? rgba8(30, 30, 30) : rgba8(255, 255, 255);
    const uint32_t fg = dark ? rgba8(220, 220, 220) : rgba8(30, 30, 30);
    const uint32_t grid = dark ? rgba8(80, 80, 80, 130) : rgba8(200, 200, 200, 180);

    int margin_l = 80 * scale;
    int margin_b = 60 * scale;
    int margin_t = 30 * scale;
    int margin_r = 30 * scale;

    bool have_bounds = false;
    double u_min = 0.0, u_max = 0.0;
    if (last_pde_) {
        auto bounds = last_pde_->get_global_bounds();
        u_min = bounds.first;
        u_max = bounds.second;
        have_bounds = (u_min <= u_max);
    }

    const bool draw_colorbar = export_settings_.include_colorbar && have_bounds && export_settings_.include_solution;
    const int estimated_plot_h = std::max(64, (int)std::lround(state_.size.y * (float)scale) - margin_t - margin_b);
    ColorbarLayout colorbar_layout;
    if (draw_colorbar) {
        colorbar_layout = make_colorbar_layout((float)estimated_plot_h, scale);
        margin_r = std::max(margin_r, colorbar_layout.right_margin);
    }

    const double world_w = std::max(1e-12, wx_max - wx_min);
    const double world_h = std::max(1e-12, wy_max - wy_min);
    const double world_aspect = world_w / world_h;
    
    int base_plot_w = (int)std::lround(state_.size.x * (float)scale) - margin_l - margin_r;
    int base_plot_h = (int)std::lround(state_.size.y * (float)scale) - margin_t - margin_b;
    base_plot_w = std::max(64, base_plot_w);
    base_plot_h = std::max(64, base_plot_h);
    
    const double screen_aspect = (double)base_plot_w / (double)base_plot_h;
    int plot_w, plot_h;
    if (world_aspect > screen_aspect) {
        plot_w = base_plot_w;
        plot_h = (int)std::lround(plot_w / world_aspect);
    } else {
        plot_h = base_plot_h;
        plot_w = (int)std::lround(plot_h * world_aspect);
    }
    
    const int W = base_plot_w + margin_l + margin_r;
    const int H = base_plot_h + margin_t + margin_b;
    
    const int plot_x0 = margin_l + (base_plot_w - plot_w) / 2;
    const int plot_y0 = margin_t + (base_plot_h - plot_h) / 2;

    if (draw_colorbar) {
        colorbar_layout = make_colorbar_layout((float)plot_h, scale);
    }

    auto to_px = [&](double wx, double wy) -> ImVec2 {
        const double tx = (wx - wx_min) / world_w;
        const double ty = (wy_max - wy) / world_h;
        return ImVec2(
            (float)(plot_x0 + tx * plot_w),
            (float)(plot_y0 + ty * plot_h)
        );
    };

    auto color_for_u = [&](double u) -> uint32_t {
        if (!have_bounds) return rgba8(128, 128, 128, 220);
        double t = (u_max > u_min) ? (u - u_min) / (u_max - u_min) : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        struct Stop { double t; uint8_t r, g, b; };
        constexpr Stop stops[] = {
            {0.00,  53,  42, 135},
            {0.25,  15, 111, 198},
            {0.50,   3, 167, 133},
            {0.75, 144, 205,  57},
            {1.00, 249, 251,  21},
        };
        int seg = 0;
        for (int i = 0; i < 4; ++i) { if (t >= stops[i+1].t) seg = i+1; else break; }
        seg = std::min(seg, 3);
        const auto& s0 = stops[seg];
        const auto& s1 = stops[seg+1];
        double lt = (t - s0.t) / (s1.t - s0.t);
        lt = std::clamp(lt, 0.0, 1.0);
        uint8_t r = (uint8_t)std::lround(s0.r + lt * (s1.r - s0.r));
        uint8_t g = (uint8_t)std::lround(s0.g + lt * (s1.g - s0.g));
        uint8_t b = (uint8_t)std::lround(s0.b + lt * (s1.b - s0.b));
        return rgba8(r, g, b, 230);
    };

    // Allocate RGBA buffer
    std::vector<uint32_t> pixels(W * H, bg);

    auto set_pixel = [&](int x, int y, uint32_t color) {
        if (x >= 0 && x < W && y >= 0 && y < H) {
            pixels[y * W + x] = color;
        }
    };

    auto blend_pixel = [&](int x, int y, uint32_t src) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        uint32_t& dst = pixels[y * W + x];
        uint8_t sa = (src >> 24) & 0xFF;
        if (sa == 255) {
            dst = src;
        } else if (sa > 0) {
            uint8_t sr = src & 0xFF;
            uint8_t sg = (src >> 8) & 0xFF;
            uint8_t sb = (src >> 16) & 0xFF;
            uint8_t dr = dst & 0xFF;
            uint8_t dg = (dst >> 8) & 0xFF;
            uint8_t db = (dst >> 16) & 0xFF;
            uint8_t da = (dst >> 24) & 0xFF;
            float alpha = sa / 255.0f;
            uint8_t r = (uint8_t)(sr * alpha + dr * (1.0f - alpha));
            uint8_t g = (uint8_t)(sg * alpha + dg * (1.0f - alpha));
            uint8_t b = (uint8_t)(sb * alpha + db * (1.0f - alpha));
            uint8_t a = std::max(sa, da);
            dst = rgba8(r, g, b, a);
        }
    };

    auto draw_line = [&](int x0, int y0, int x1, int y1, uint32_t color, int thickness = 1) {
        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;
        int x = x0, y = y0;

        while (true) {
            for (int ty = -thickness/2; ty <= thickness/2; ++ty) {
                for (int tx = -thickness/2; tx <= thickness/2; ++tx) {
                    blend_pixel(x + tx, y + ty, color);
                }
            }
            if (x == x1 && y == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    };

    auto fill_triangle = [&](ImVec2 a, ImVec2 b, ImVec2 c, uint32_t color) {
        int x0 = (int)std::lround(a.x), y0 = (int)std::lround(a.y);
        int x1 = (int)std::lround(b.x), y1 = (int)std::lround(b.y);
        int x2 = (int)std::lround(c.x), y2 = (int)std::lround(c.y);

        int min_x = std::max(0, std::min({x0, x1, x2}));
        int max_x = std::min(W - 1, std::max({x0, x1, x2}));
        int min_y = std::max(0, std::min({y0, y1, y2}));
        int max_y = std::min(H - 1, std::max({y0, y1, y2}));

        auto sign = [](int px, int py, int ax, int ay, int bx, int by) -> float {
            return (float)((px - bx) * (ay - by) - (ax - bx) * (py - by));
        };

        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                float d1 = sign(x, y, x0, y0, x1, y1);
                float d2 = sign(x, y, x1, y1, x2, y2);
                float d3 = sign(x, y, x2, y2, x0, y0);
                bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
                if (!(has_neg && has_pos)) {
                    blend_pixel(x, y, color);
                }
            }
        }
    };

    auto draw_circle = [&](int cx, int cy, int radius, uint32_t color) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (dx*dx + dy*dy <= radius*radius) {
                    set_pixel(cx + dx, cy + dy, color);
                }
            }
        }
    };

    if (export_settings_.include_solution && last_pde_ && have_bounds) {
        const DifferentialEquationSolution& sol = last_pde_->solution(last_mesh_);
        if (sol.is_ready() && sol.solution_u.size() == R.points.size()) {
            for (const auto& tri : R.triangles) {
                if (!tri.valid) continue;
                const auto& A = R.points[tri.v[0]];
                const auto& B = R.points[tri.v[1]];
                const auto& C = R.points[tri.v[2]];
                const double uavg = (sol.solution_u[tri.v[0]] + sol.solution_u[tri.v[1]] + sol.solution_u[tri.v[2]]) / 3.0;
                const uint32_t col = color_for_u(uavg);
                const ImVec2 pA = to_px(A.x(), A.y());
                const ImVec2 pB = to_px(B.x(), B.y());
                const ImVec2 pC = to_px(C.x(), C.y());
                fill_triangle(pA, pB, pC, col);
            }
        }
    }

    auto bc_color = [&](BoundaryConditionType::Type t) -> uint32_t {
        switch (t) {
        case BoundaryConditionType::Type::Dirichlet: return dark ? rgba8(255, 120, 200, 255) : rgba8(180, 0, 140, 255);
        case BoundaryConditionType::Type::Neumann:   return dark ? rgba8(80, 220, 255, 255) : rgba8(0, 130, 180, 255);
        case BoundaryConditionType::Type::Robin:     return dark ? rgba8(255, 210, 80, 255) : rgba8(200, 130, 0, 255);
        default:                               return dark ? rgba8(120, 120, 120, 255) : rgba8(100, 100, 100, 255);
        }
    };

    if (export_settings_.include_mesh) {
        const int thin = std::max(1, (scale > 2) ? scale / 2 : 1);
        const int thick = std::max(1, scale);
        
        const bool solution_visible = export_settings_.include_solution && last_pde_ && have_bounds;
        const uint32_t edge_col = solution_visible 
                                  ? (dark ? rgba8(200, 200, 200, 160) : rgba8(80, 80, 80, 120))
                                  : (dark ? rgba8(100, 100, 100, 200) : rgba8(50, 50, 50, 255));
        const uint32_t boundary_col = dark ? rgba8(140, 200, 140, 240) : rgba8(30, 30, 30, 255);
        
        for (const EdgeInfo& e : R.edges) {
            if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;
            const auto& A = R.points[e.a];
            const auto& B = R.points[e.b];
            const ImVec2 pA = to_px(A.x(), A.y());
            const ImVec2 pB = to_px(B.x(), B.y());
            const uint32_t c = e.on_boundary ? boundary_col : edge_col;
            const int w = e.on_boundary ? thick : thin;
            draw_line((int)pA.x, (int)pA.y, (int)pB.x, (int)pB.y, c, w);
        }
    }

    if (export_settings_.include_boundary_conditions && last_mesh_) {
        const int bc_thickness = std::max(2, (int)(2.5f * scale));
        for (BoundaryCondition* bc : last_mesh_->boundary_conditions()) {
            const uint32_t color = bc_color(bc->type().value);
            for (int eid : bc->edge_ids()) {
                if ((size_t)eid >= R.edges.size()) continue;
                const EdgeInfo& e = R.edges[eid];
                if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;
                const auto& A = R.points[e.a];
                const auto& B = R.points[e.b];
                const ImVec2 pA = to_px(A.x(), A.y());
                const ImVec2 pB = to_px(B.x(), B.y());
                draw_line((int)pA.x, (int)pA.y, (int)pB.x, (int)pB.y, color, bc_thickness);
            }
        }
    }

    if (export_settings_.include_points) {
        const int r = std::max(1, (int)std::lround(point_radius_ * (float)scale * 0.85f));
        const uint32_t in_col = dark ? rgba8(230, 120, 120, 230) : rgba8(180, 50, 50, 240);
        const uint32_t bd_col = dark ? rgba8(100, 200, 120, 230) : rgba8(40, 120, 50, 240);
        for (const Point2D& pt : R.points) {
            const ImVec2 p = to_px(pt.x(), pt.y());
            const uint32_t c = pt.on_boundary ? bd_col : in_col;
            draw_circle((int)p.x, (int)p.y, r, c);
        }
    }

    if (export_settings_.include_axes) {
        const int frame_w = std::max(1, (int)(1.5 * scale));
        draw_line(plot_x0, plot_y0, plot_x0 + plot_w, plot_y0, fg, frame_w);
        draw_line(plot_x0 + plot_w, plot_y0, plot_x0 + plot_w, plot_y0 + plot_h, fg, frame_w);
        draw_line(plot_x0 + plot_w, plot_y0 + plot_h, plot_x0, plot_y0 + plot_h, fg, frame_w);
        draw_line(plot_x0, plot_y0 + plot_h, plot_x0, plot_y0, fg, frame_w);
    }

    auto draw_text_5x7 = [&](int x, int y, std::string_view text, uint32_t color, int px_scale, int anchor /*0=left,1=mid,2=right*/) {
        ImVec2 sz = measure_text_5x7(text, px_scale);
        int xx = x;
        if (anchor == 1) xx -= (int)std::lround(sz.x * 0.5f);
        if (anchor == 2) xx -= (int)std::lround(sz.x);

        const int cw = 5 * px_scale;
        const int ch = 7 * px_scale;
        const int sp = 1 * px_scale;

        int pen_x = xx;
        uint8_t rows[7];
        for (char c : text) {
            if (!glyph_5x7(c, rows)) {
                pen_x += cw + sp;
                continue;
            }
            for (int ry = 0; ry < 7; ++ry) {
                for (int rx = 0; rx < 5; ++rx) {
                    if (rows[ry] & (1u << (4 - rx))) {
                        for (int sy = 0; sy < px_scale; ++sy) {
                            for (int sx = 0; sx < px_scale; ++sx) {
                                blend_pixel(pen_x + rx * px_scale + sx, y + ry * px_scale + sy, color);
                            }
                        }
                    }
                }
            }
            pen_x += cw + sp;
        }
        (void)ch;
    };

    if (export_settings_.include_axes) {
        const int tick_len = 5 * scale;
        const int label_scale = std::max(1, scale);

        // X-axis ticks (rounded via nice_ticks)
        const auto x_ticks = nice_ticks(wx_min, wx_max, 6);
        for (double wx : x_ticks) {
            double t = (wx - wx_min) / (wx_max - wx_min);
            int x = (int)(plot_x0 + t * plot_w);
            int y0 = plot_y0 + plot_h;
            int y1 = y0 + tick_len;
            draw_line(x, y0, x, y1, fg, scale);

            std::string label = fmt_tick(wx);
            draw_text_5x7(x, y1 + 4 * scale, label, fg, label_scale, 1);
        }

        // Y-axis ticks (rounded via nice_ticks)
        const auto y_ticks = nice_ticks(wy_min, wy_max, 6);
        for (double wy : y_ticks) {
            double t = (wy - wy_min) / (wy_max - wy_min);
            int y = (int)(plot_y0 + (1.0 - t) * plot_h);
            int x0 = plot_x0;
            int x1 = x0 - tick_len;
            draw_line(x0, y, x1, y, fg, scale);

            std::string label = fmt_tick(wy);
            draw_text_5x7(x1 - 4 * scale, y - (int)std::lround(3.5f * label_scale), label, fg, label_scale, 2);
        }
    }

    if (draw_colorbar) {
        const int cb_w = (int)std::lround(colorbar_layout.width);
        const int cb_h = (int)std::lround(std::min<float>(plot_h, colorbar_layout.height));
        const int cb_x = plot_x0 + plot_w + (int)std::lround(colorbar_layout.x_gap);
        const int cb_y = plot_y0 + (int)std::lround(colorbar_layout.y_offset);

        for (int y = 0; y < cb_h; ++y) {
            double t = 1.0 - (double)y / (double)(cb_h - 1);
            double u = u_min + t * (u_max - u_min);
            uint32_t col = color_for_u(u);
            for (int x = 0; x < cb_w; ++x) {
                set_pixel(cb_x + x, cb_y + y, col);
            }
        }

        draw_line(cb_x, cb_y, cb_x + cb_w, cb_y, fg, scale);
        draw_line(cb_x + cb_w, cb_y, cb_x + cb_w, cb_y + cb_h, fg, scale);
        draw_line(cb_x + cb_w, cb_y + cb_h, cb_x, cb_y + cb_h, fg, scale);
        draw_line(cb_x, cb_y + cb_h, cb_x, cb_y, fg, scale);

        const int label_scale = colorbar_layout.bitmap_label_scale;
        const std::string u_max_label = fmt_tick(u_max);
        const std::string u_min_label = fmt_tick(u_min);
        const ImVec2 u_max_label_size = measure_text_5x7(u_max_label, label_scale);
        const ImVec2 u_min_label_size = measure_text_5x7(u_min_label, label_scale);
        draw_text_5x7(
            cb_x + cb_w + (int)std::lround(colorbar_layout.label_gap),
            cb_y,
            u_max_label,
            fg,
            label_scale,
            0
        );
        draw_text_5x7(
            cb_x + cb_w + (int)std::lround(colorbar_layout.label_gap),
            cb_y + cb_h - (int)std::lround(u_min_label_size.y),
            u_min_label,
            fg,
            label_scale,
            0
        );
        (void)u_max_label_size;
    }

    if (export_settings_.include_bc_legend && export_settings_.include_boundary_conditions && last_mesh_) {
        auto bc_name = [](BoundaryConditionType::Type t) -> const char* {
            switch (t) {
            case BoundaryConditionType::Type::Dirichlet: return "Dirichlet";
            case BoundaryConditionType::Type::Neumann:   return "Neumann";
            case BoundaryConditionType::Type::Robin:     return "Robin";
            default:                               return "Unknown";
            }
        };

        const int legend_x = 10 * scale;
        const int legend_y = plot_y0 + plot_h - 10 * scale;
        const int line_h = 12 * scale;
        const int line_w = 20 * scale;
        const int line_thickness = std::max(2, (int)(2.5f * scale));
        const int text_scale = std::max(1, scale);
        
        int y_offset = 0;
        std::vector<BoundaryConditionType> bc_types;
        for (BoundaryCondition* bc : last_mesh_->boundary_conditions()) {
            bool found = false;
            for (BoundaryConditionType existing : bc_types) {
                if (existing == bc->type()) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                bc_types.push_back(bc->type());
            }
        }
        
        for (BoundaryConditionType bc_type : bc_types) {
            const uint32_t color = bc_color(bc_type.value);
            const int y = legend_y - y_offset;
            const int x1 = legend_x;
            const int x2 = legend_x + line_w;
            
            draw_line(x1, y, x2, y, color, line_thickness);
            draw_text_5x7(x2 + 5 * scale, y - 3 * text_scale, bc_name(bc_type.value), fg, text_scale, 0);
            
            y_offset += line_h;
        }
    }

    int result = stbi_write_png(absolute_path.c_str(), W, H, 4, pixels.data(), W * 4);
    return result != 0;
}

void CanvasWindow::draw_selection_overlay(const CanvasWindowDrawInfo& draw_info) {
    if (!draw_info.selected_mesh) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const DelaunayTriangulationResult& R = draw_info.selected_mesh->triangulation_result();

    BoundaryCondition* edited_bc = draw_info.selected_mesh->edited_boundary_condition();

    if (edited_bc && !edited_bc->edge_ids().empty()) {
        constexpr float kEditedBCThickness = 5.0f;
        constexpr ImU32 kEditedBCColor = IM_COL32(0, 255, 0, 255);  // Bright green

        for (int eid : edited_bc->edge_ids()) {
            if (eid < 0 || (size_t)eid >= R.edges.size()) continue;
            
            const EdgeInfo& e = R.edges[eid];
            if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;

            const auto& A = R.points[e.a];
            const auto& B = R.points[e.b];
            ImVec2 pA = viewport_.to_screen({A.x(), A.y()});
            ImVec2 pB = viewport_.to_screen({B.x(), B.y()});
            
            draw_list->AddLine(pA, pB, kEditedBCColor, kEditedBCThickness);
        }
    }

    if (!edited_bc) {
        constexpr float kSelectionThickness = 5.0f;
        constexpr ImU32 kSelectionColor = IM_COL32(255, 255, 0, 255);  // Yellow

        for (int eid : selected_edge_ids_) {
            if (eid < 0 || (size_t)eid >= R.edges.size()) continue;
            
            const EdgeInfo& e = R.edges[eid];
            if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;

            const auto& A = R.points[e.a];
            const auto& B = R.points[e.b];
            ImVec2 pA = viewport_.to_screen({A.x(), A.y()});
            ImVec2 pB = viewport_.to_screen({B.x(), B.y()});
            
            draw_list->AddLine(pA, pB, kSelectionColor, kSelectionThickness);
        }
    }

    constexpr float kHoverThickness = 4.0f;
    constexpr ImU32 kHoverColor = IM_COL32(0, 255, 255, 200);  // Cyan

    for (int eid : hovered_edge_ids_) {
        if (selected_edge_ids_.count(eid) > 0) continue;  // Don't re-draw selected
        if (edited_bc && std::find(edited_bc->edge_ids().begin(), edited_bc->edge_ids().end(), eid) != edited_bc->edge_ids().end()) continue;
        if (eid < 0 || (size_t)eid >= R.edges.size()) continue;
        
        const EdgeInfo& e = R.edges[eid];
        if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;

        const auto& A = R.points[e.a];
        const auto& B = R.points[e.b];
        ImVec2 pA = viewport_.to_screen({A.x(), A.y()});
        ImVec2 pB = viewport_.to_screen({B.x(), B.y()});
        
        draw_list->AddLine(pA, pB, kHoverColor, kHoverThickness);
    }
}

void CanvasWindow::draw_rubber_band() {
    if (!is_rubber_band_active_) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 min_pos(std::min(rubber_band_start_.x, rubber_band_end_.x),
                   std::min(rubber_band_start_.y, rubber_band_end_.y));
    ImVec2 max_pos(std::max(rubber_band_start_.x, rubber_band_end_.x),
                   std::max(rubber_band_start_.y, rubber_band_end_.y));

    draw_list->AddRectFilled(min_pos, max_pos, IM_COL32(100, 150, 255, 50));
    
    draw_list->AddRect(min_pos, max_pos, IM_COL32(100, 150, 255, 255), 0.0f, 0, 2.0f);
}

std::vector<int> CanvasWindow::pick_edges_in_rect(const DelaunayTriangulationResult& R,
                                                   const glm::vec2& screen_min,
                                                   const glm::vec2& screen_max)
{
    std::vector<int> picked_edges;

    glm::dvec2 world_min = viewport_.to_world(ImVec2(screen_min.x, screen_min.y));
    glm::dvec2 world_max = viewport_.to_world(ImVec2(screen_max.x, screen_max.y));

    double wx_min = std::min(world_min.x, world_max.x);
    double wx_max = std::max(world_min.x, world_max.x);
    double wy_min = std::min(world_min.y, world_max.y);
    double wy_max = std::max(world_min.y, world_max.y);

    for (size_t i = 0; i < R.edges.size(); ++i) {
        const EdgeInfo& e = R.edges[i];
        if (!e.on_boundary) continue;  // Only select boundary edges
        
        if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;

        const auto& A = R.points[e.a];
        const auto& B = R.points[e.b];

        double mx = (A.x() + B.x()) * 0.5;
        double my = (A.y() + B.y()) * 0.5;

        if (mx >= wx_min && mx <= wx_max && my >= wy_min && my <= wy_max) {
            picked_edges.push_back((int)i);
        }
    }

    return picked_edges;
}

void CanvasWindow::start_rubber_band_selection(const glm::vec2& screen_pos) {
    is_rubber_band_active_ = true;
    rubber_band_start_ = screen_pos;
    rubber_band_end_ = screen_pos;
}

void CanvasWindow::update_rubber_band_selection(const glm::vec2& screen_pos) {
    if (!is_rubber_band_active_) return;
    rubber_band_end_ = screen_pos;
}

void CanvasWindow::finish_rubber_band_selection(const CanvasWindowDrawInfo& draw_info) {
    if (!is_rubber_band_active_ || !draw_info.selected_mesh) {
        is_rubber_band_active_ = false;
        return;
    }

    const DelaunayTriangulationResult& R = draw_info.selected_mesh->triangulation_result();
    
    glm::vec2 min_pos(std::min(rubber_band_start_.x, rubber_band_end_.x),
                      std::min(rubber_band_start_.y, rubber_band_end_.y));
    glm::vec2 max_pos(std::max(rubber_band_start_.x, rubber_band_end_.x),
                      std::max(rubber_band_start_.y, rubber_band_end_.y));

    std::vector<int> picked = pick_edges_in_rect(R, min_pos, max_pos);

    if (BoundaryCondition* edited_bc = draw_info.selected_mesh->edited_boundary_condition()) {
        ImGuiIO& io = ImGui::GetIO();
        std::vector<int> new_edges;
        
        if (io.KeyCtrl) {
            new_edges = edited_bc->edge_ids();
            for (int eid : picked) {
                if (std::find(new_edges.begin(), new_edges.end(), eid) == new_edges.end()) {
                    new_edges.push_back(eid);
                }
            }
        } else {
            new_edges = picked;
        }
        
        edited_bc->set_edge_ids(new_edges);
        
    } else {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.KeyCtrl) {
            clear_edge_selection();
        }

        for (int eid : picked) {
            add_edge_to_selection(eid);
        }
    }

    is_rubber_band_active_ = false;
}

void CanvasWindow::cancel_rubber_band_selection() {
    is_rubber_band_active_ = false;
}

void CanvasWindow::clear_edge_selection() {
    selected_edge_ids_.clear();
}

void CanvasWindow::add_edge_to_selection(int edge_id) {
    selected_edge_ids_.insert(edge_id);
}

void CanvasWindow::remove_edge_from_selection(int edge_id) {
    selected_edge_ids_.erase(edge_id);
}

void CanvasWindow::toggle_edge_selection(int edge_id) {
    if (selected_edge_ids_.count(edge_id) > 0) {
        remove_edge_from_selection(edge_id);
    } else {
        add_edge_to_selection(edge_id);
    }
}

bool CanvasWindow::is_edge_selected(int edge_id) const {
    return selected_edge_ids_.count(edge_id) > 0;
}

}