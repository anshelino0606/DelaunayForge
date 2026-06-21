#include "planar_mesh_generator_window.h"
#include "canvas_window.h"
#include "geom/triangulation_session_events.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "log_categories.h"

#include <imgui/imgui.h>

namespace fem {

void commit_boundary_input(
    PlanarMeshComponent& mesh,
    const std::vector<Point2D>& loop,
    fem::DelaunayMeshGeneratorMode mode,
    bool multi_loop_enabled)
{
    if (mode == fem::DelaunayMeshGeneratorMode::POLYGON_CLIP) {
        mesh.set_polygon_points(loop, true);
        mesh.clear_boundary_loops(); // don’t keep stale boundary
        return;
    }

    if (mode == fem::DelaunayMeshGeneratorMode::WITH_BOUNDARY) {
        mesh.clear_polygon_points(); // don’t keep stale polygon

        if (multi_loop_enabled) mesh.add_boundary_loop(loop, true);
        else mesh.set_single_boundary_loop(loop, true);

        return;
    }
}


PlanarMeshGeneratorWindow::PlanarMeshGeneratorWindow() {
    DelaunayMeshGeneratorConfig& mesh_generator_config = triangulation_session_config_.mesh_generator_config;

    mesh_generator_config.min_angle_threshold        = 0.0f;
    mesh_generator_config.enable_lloyd_smoothing     = false;
    mesh_generator_config.lloyd_iterations           = 3;
    mesh_generator_config.enable_edge_flipping       = false;
}

void PlanarMeshGeneratorWindow::draw(const PlanarMeshGeneratorWindowDrawInfo& draw_info) {
}

void PlanarMeshGeneratorWindow::send_triangulation_request(const PlanarMeshGeneratorWindowDrawInfo& draw_info) {
    FEM_TRIGGER_EVENT(PlanarTriangulationRequest, triangulation_session_config_);
    triangulation_session_config_.density_function = nullptr;
}

void PlanarMeshGeneratorWindow::draw_parametric_controls(const PlanarMeshGeneratorWindowDrawInfo& draw_info) {
    // if (state_.boundary_type != BoundaryInputType::Parametric) {
    //     return;
    // }
    
    ImGui::Separator();
    ImGui::Text("Parametric Curve Settings:");
    
    ParametricCurveConfig& cfg = state_.parametric_cfg;
    
    const char* preset_names[] = {
        "Circle",
        "Ellipse", 
        "Cardioid",
        "Lemniscate",
        "Epicycloid",
        "Hypocycloid",
        "Rose",
        "Spiral",
        "Custom"
    };
    
    int preset_idx = (int)cfg.preset;
    if (ImGui::Combo("Preset", &preset_idx, preset_names, IM_ARRAYSIZE(preset_names))) {
        cfg.preset = (ParametricPreset)preset_idx;
        
        // Set sensible defaults for each preset
        const double pi = 3.14159265358979323846;
        switch (cfg.preset) {
            case ParametricPreset::Circle:
                cfg.a = 100.0; cfg.t_start = 0.0; cfg.t_end = 2*pi;
                break;
            case ParametricPreset::Ellipse:
                cfg.a = 150.0; cfg.b = 80.0; cfg.t_start = 0.0; cfg.t_end = 2*pi;
                break;
            case ParametricPreset::Cardioid:
                cfg.a = 80.0; cfg.t_start = 0.0; cfg.t_end = 2*pi;
                break;
            case ParametricPreset::Lemniscate:
                cfg.a = 100.0; cfg.t_start = -pi; cfg.t_end = pi;
                break;
            case ParametricPreset::Epicycloid:
                cfg.a = 100.0; cfg.b = 30.0; cfg.t_start = 0.0; cfg.t_end = 2*pi;
                break;
            case ParametricPreset::Hypocycloid:
                cfg.a = 120.0; cfg.b = 40.0; cfg.t_start = 0.0; cfg.t_end = 2*pi;
                break;
            case ParametricPreset::Spiral:
                cfg.a = 20.0; cfg.b = 5.0; cfg.t_start = 0.0; cfg.t_end = 6*pi;
                break;
            case ParametricPreset::Custom:
                cfg.custom_x_expr = "a*cos(t)";
                cfg.custom_y_expr = "a*sin(t)";
                break;
        }
    }
    
    // Show appropriate parameters based on preset
    bool params_changed = false;
    
    if (cfg.preset != ParametricPreset::Custom) {
        switch (cfg.preset) {
            case ParametricPreset::Circle:
                params_changed |= ImGui::SliderFloat("Radius", (float*)&cfg.a, 10.0f, 300.0f);
                break;
                
            case ParametricPreset::Ellipse:
                params_changed |= ImGui::SliderFloat("Width (a)", (float*)&cfg.a, 10.0f, 300.0f);
                params_changed |= ImGui::SliderFloat("Height (b)", (float*)&cfg.b, 10.0f, 300.0f);
                break;
                
            case ParametricPreset::Cardioid:
                params_changed |= ImGui::SliderFloat("Size (a)", (float*)&cfg.a, 10.0f, 200.0f);
                break;
                
            case ParametricPreset::Lemniscate:
                params_changed |= ImGui::SliderFloat("Size (a)", (float*)&cfg.a, 10.0f, 200.0f);
                break;
                
            case ParametricPreset::Epicycloid:
            case ParametricPreset::Hypocycloid:
                params_changed |= ImGui::SliderFloat("Outer radius (a)", (float*)&cfg.a, 20.0f, 200.0f);
                params_changed |= ImGui::SliderFloat("Inner radius (b)", (float*)&cfg.b, 5.0f, 100.0f);
                ImGui::TextDisabled("Try: a/b = integer for interesting shapes");
                break;
                
            case ParametricPreset::Spiral:
                params_changed |= ImGui::SliderFloat("Start radius (a)", (float*)&cfg.a, 0.0f, 100.0f);
                params_changed |= ImGui::SliderFloat("Growth rate (b)", (float*)&cfg.b, 0.5f, 20.0f);
                break;
                
            default:
                break;
        }
    }
    
    if (cfg.preset == ParametricPreset::Custom) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Expression Syntax:");
        ImGui::TextDisabled("Variables: t, a, b, c, pi, e");
        ImGui::TextDisabled("Operators: +, -, *, /, ^(power), %%(mod)");
        ImGui::TextDisabled("Functions: sin, cos, tan, asin, acos, atan");
        ImGui::TextDisabled("          exp, log, ln, sqrt, abs, floor, ceil");
        ImGui::TextDisabled("          pow(x,y), min(x,y), max(x,y), atan2(y,x)");
        ImGui::Separator();
        
        char x_buf[256], y_buf[256];
        strncpy(x_buf, cfg.custom_x_expr.c_str(), sizeof(x_buf) - 1);
        strncpy(y_buf, cfg.custom_y_expr.c_str(), sizeof(y_buf) - 1);
        x_buf[255] = y_buf[255] = '\0';
        
        bool x_changed = false, y_changed = false;
        
        if (ImGui::InputText("x(t)", x_buf, sizeof(x_buf))) {
            cfg.custom_x_expr = x_buf;
            x_changed = true;
        }
        
        if (!cfg.custom_x_expr.empty()) {
            if (parametric_tool_.last_error().empty()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", 
                                 parametric_tool_.last_error().c_str());
            }
        }
        
        if (ImGui::InputText("y(t)", y_buf, sizeof(y_buf))) {
            cfg.custom_y_expr = y_buf;
            y_changed = true;
        }
        
        // Validate y expression
        if (!cfg.custom_y_expr.empty()) {
            if (parametric_tool_.last_error().empty()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", 
                                 parametric_tool_.last_error().c_str());
            }
        }
        
        params_changed = x_changed || y_changed;
        
        params_changed |= ImGui::SliderFloat("Parameter a", (float*)&cfg.a, 1.0f, 300.0f);
        params_changed |= ImGui::SliderFloat("Parameter b", (float*)&cfg.b, 1.0f, 300.0f);
        params_changed |= ImGui::SliderFloat("Parameter c", (float*)&cfg.c, 0.1f, 10.0f);
        
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Example Curves:");
        
        if (ImGui::Button("Circle")) {
            cfg.custom_x_expr = "a*cos(t)";
            cfg.custom_y_expr = "a*sin(t)";
            params_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Ellipse")) {
            cfg.custom_x_expr = "a*cos(t)";
            cfg.custom_y_expr = "b*sin(t)";
            params_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Figure-8")) {
            cfg.custom_x_expr = "a*sin(t)";
            cfg.custom_y_expr = "a*sin(t)*cos(t)";
            params_changed = true;
        }
        
        if (ImGui::Button("Superellipse")) {
            cfg.custom_x_expr = "a*pow(abs(cos(t)), 2/3)*sign(cos(t))";
            cfg.custom_y_expr = "b*pow(abs(sin(t)), 2/3)*sign(sin(t))";
            params_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Astroid")) {
            cfg.custom_x_expr = "a*pow(cos(t), 3)";
            cfg.custom_y_expr = "a*pow(sin(t), 3)";
            params_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Deltoid")) {
            cfg.custom_x_expr = "2*a*cos(t) + a*cos(2*t)";
            cfg.custom_y_expr = "2*a*sin(t) - a*sin(2*t)";
            params_changed = true;
        }
        
        if (ImGui::Button("Lissajous (3:2)")) {
            cfg.custom_x_expr = "a*sin(3*t)";
            cfg.custom_y_expr = "b*sin(2*t)";
            params_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Butterfly")) {
            cfg.custom_x_expr = "a*sin(t)*(exp(cos(t)) - 2*cos(4*t) - pow(sin(t/12), 5))";
            cfg.custom_y_expr = "a*cos(t)*(exp(cos(t)) - 2*cos(4*t) - pow(sin(t/12), 5))";
            cfg.t_start = 0.0;
            cfg.t_end = 12.0 * 3.14159265358979323846;
            params_changed = true;
        }
    }
    
    ImGui::Separator();
    const double pi = 3.14159265358979323846;
    float t_start_f = (float)cfg.t_start;
    float t_end_f = (float)cfg.t_end;
    
    params_changed |= ImGui::SliderFloat("t start", &t_start_f, -4.0f*(float)pi, 4.0f*(float)pi, "%.3f");
    params_changed |= ImGui::SliderFloat("t end", &t_end_f, -4.0f*(float)pi, 8.0f*(float)pi, "%.3f");
    
    cfg.t_start = t_start_f;
    cfg.t_end = t_end_f;
    
    params_changed |= ImGui::SliderInt("Sample count", &cfg.sample_count, 16, 512);
    
    if (draw_info.canvas_state) {
        if (cfg.center.x == 0.0 && cfg.center.y == 0.0) {
            cfg.center.x = draw_info.canvas_state->size.x * 0.5;
            cfg.center.y = draw_info.canvas_state->size.y * 0.5;
        }
        
        float center_x = (float)cfg.center.x;
        float center_y = (float)cfg.center.y;
        
        params_changed |= ImGui::SliderFloat("Center X", &center_x, 0.0f, draw_info.canvas_state->size.x);
        params_changed |= ImGui::SliderFloat("Center Y", &center_y, 0.0f, draw_info.canvas_state->size.y);
        
        cfg.center.x = center_x;
        cfg.center.y = center_y;
    }
    
    if (ImGui::Button("Generate Parametric Boundary", ImVec2(-1, 0))) {
        generate_parametric_boundary(draw_info);
    }
    
    if (params_changed && ImGui::GetIO().KeyCtrl) {
        generate_parametric_boundary(draw_info);
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Hold Ctrl while adjusting sliders for live preview");
    }
}

void PlanarMeshGeneratorWindow::generate_parametric_boundary(const PlanarMeshGeneratorWindowDrawInfo& draw_info) {
    if (!draw_info.selected_mesh) {
        LOGT_ERROR(LogEditor, "No mesh selected for parametric boundary");
        return;
    }
    
    auto points = parametric_tool_.generate(state_.parametric_cfg);
    
    if (points.empty()) {
        LOGT_ERROR(LogEditor, "Failed to generate parametric curve");
        return;
    }
    
    LOGT_DEBUG(LogEditor, "Generated parametric boundary with %zu points", points.size());
    
    PlanarMeshComponent* mesh = draw_info.selected_mesh;
    
    triangulation_session_config_.mesh_generator_config.mode = 
        fem::DelaunayMeshGeneratorMode::WITH_BOUNDARY;
    
    mesh->clear_polygon_points();
    mesh->clear_boundary_points();
    
    if (state_.multi_loop_mode) {
        mesh->add_boundary_loop(points, /*mark_boundary=*/true);
        LOGT_DEBUG(LogEditor, "Added parametric boundary loop, total loops: %zu", 
                   mesh->boundary_loops().size());
    } else {
        mesh->set_single_boundary_loop(points, /*mark_boundary=*/true);
        LOGT_DEBUG(LogEditor, "Set single parametric boundary loop");
    }
    
    send_triangulation_request(draw_info);
}

}