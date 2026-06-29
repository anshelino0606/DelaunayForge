#include "canvas_window.h"
#include "planar_mesh_generator_window.h"
#include "core/entity/entity.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "geom/planar_mesh/planar_mesh_boundary_base.h"
#include "geom/planar_mesh/planar_mesh_outer_boundary.h"
#include "geom/triangulation/triangulation_session_events.h"
#include "math/boundary_condition.h"
#include "math/bc_group_manager.h"
#include "math/differential_equation_solution.h"
#include "math/pde/pde_component.h"
#include "math/entities/planar_math_entity.h"
#include "tools/bc_utils.h"
#include "plot/png.h"
#include "plot/svg.h"
#include "plot/utils.h"
#include "log_categories.h"
#include "core/file_system/file_system.h"
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cstring>

namespace fem {

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

    std::string u_min_text = "u_min=" + plot::fmt_no_plus(global_u_min);
    std::string u_max_text = "u_max=" + plot::fmt_no_plus(global_u_max);

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
        int theme_idx = (export_settings_.theme == plot::ExportSettings::Theme::Dark) ? 0 : 1;
        if (ImGui::Combo("Theme", &theme_idx, themes, IM_ARRAYSIZE(themes))) {
            export_settings_.theme = (theme_idx == 0) ? plot::ExportSettings::Theme::Dark : plot::ExportSettings::Theme::Light;
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
        ImGui::TextDisabled("u range: [%s, %s]", plot::fmt_no_plus(u_min).c_str(), plot::fmt_no_plus(u_max).c_str());
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
    return plot::export_svg(absolute_path, get_plot_scene_data());
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
        const EdgeInfo& e = R.edges[i];
        if (!e.valid_vertices(R.points.size())) 
            continue;

        const Point2D& P = R.points[e.a]; 
        const Point2D& Q = R.points[e.b];
        
        double A=x-P.x(), B=y-P.y(), C=Q.x()-P.x(), D=Q.y()-P.y();

        double len2 = C*C + D*D; 
        if (len2 < 1e-12) 
            continue;

        double t = (A*C + B*D) / len2; 
        t = std::clamp(t, 0.0, 1.0);
        
        double px = P.x() + t*C, py = P.y() + t*D;
        double dx = x - px, dy = y - py; 
        double d2 = dx*dx + dy*dy;

        if (d2 < best) { 
            best = d2; 
            best_e = (int)i; 
        }
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
    return plot::export_png(absolute_path, get_plot_scene_data());
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
            if (!e.valid_vertices(R.points.size())) continue;

            const Point2D& A = R.points[e.a];
            const Point2D& B = R.points[e.b];
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
            if (!e.valid_vertices(R.points.size())) continue;

            const Point2D& A = R.points[e.a];
            const Point2D& B = R.points[e.b];
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
        if (!e.valid_vertices(R.points.size())) continue;

        const Point2D& A = R.points[e.a];
        const Point2D& B = R.points[e.b];
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
        
        if (!e.valid_vertices(R.points.size())) continue;

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

plot::SceneData CanvasWindow::get_plot_scene_data() const {
    return {
        .last_mesh     = last_mesh_,
        .pde           = last_pde_,
        .viewport_size = state_.size,
        .point_radius  = point_radius_,
        .settings      = export_settings_
    };
}

}