#include "planar_mesh_generator_window.h"
#include "canvas_window.h"
#include "geom/triangulation_session_events.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "log_categories.h"

#include <imgui/imgui.h>
#include <random>
#include <algorithm>

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
//     if (!draw_info.selected_mesh || !draw_info.canvas_state) {
//         ImGui::Begin("Planar Mesh Generator");
//         ImGui::End();
//         return;
//     }

//     PlanarMeshComponent* mesh = draw_info.selected_mesh;
//     CanvasWindowState& canvas_state = *draw_info.canvas_state;

//     triangulation_session_config_.mesh = mesh;
//     triangulation_session_config_.density_config = mesh->density_config;

//     DelaunayMeshGeneratorConfig& delaunay_config = triangulation_session_config_.mesh_generator_config;
//     delaunay_config.enable_sizing_refinement = mesh->density_config.enable;

//     ImGui::Begin("Planar Mesh Generator");
    
//     ImGui::Text("Triangulation Mode:");
//     if (ImGui::RadioButton("Points Only", delaunay_config.mode == fem::DelaunayMeshGeneratorMode::POINTS_ONLY)) {
//         delaunay_config.mode = fem::DelaunayMeshGeneratorMode::POINTS_ONLY;
//         if (!mesh->user_points().empty()) {
//             send_triangulation_request(draw_info);
//         }
//     }

//     ImGui::SameLine();
//     if (ImGui::RadioButton("With Boundary", delaunay_config.mode == fem::DelaunayMeshGeneratorMode::WITH_BOUNDARY)) {
//         delaunay_config.mode = fem::DelaunayMeshGeneratorMode::WITH_BOUNDARY;
//         if ((!mesh->boundary_points().empty() || !mesh->polygon_points().empty() || mesh->boundary_loops().size() > 0)) {
//             send_triangulation_request(draw_info);
//         }
//     }

//     ImGui::SameLine();
//     if (ImGui::RadioButton("Polygon Clip", delaunay_config.mode == fem::DelaunayMeshGeneratorMode::POLYGON_CLIP)) {
//         delaunay_config.mode = fem::DelaunayMeshGeneratorMode::POLYGON_CLIP;
//         if (!mesh->polygon_points().empty()) {
//             send_triangulation_request(draw_info);
//         }
//     }

//     ImGui::Separator();

//     ImGui::Text("Backend:");
//     bool b = delaunay_config.backend_type == TriBackendType::GPU;
// #ifdef USE_BGFX
//     if (ImGui::RadioButton("CPU", b == 0)) {
//         delaunay_config.backend_type = TriBackendType::CPU;
//     }
//     ImGui::SameLine();
//     if (ImGui::RadioButton("GPU", b == 1)) {
//         delaunay_config.backend_type = TriBackendType::GPU;
//     }
// #else
//     ImGui::RadioButton("CPU", true);
//     ImGui::SameLine();
//     ImGui::BeginDisabled(true);
//     ImGui::RadioButton("GPU (not built)", false);
//     ImGui::EndDisabled();

//     delaunay_config.backend_type = TriBackendType::CPU;
// #endif

//     ImGui::Separator();
    
//     // Quality parameters with real-time updates
//     ImGui::Text("Quality Settings:");
//     bool config_changed = false;
    
//     if (ImGui::SliderFloat("Min Angle Threshold", &delaunay_config.min_angle_threshold, 1.0f, 45.0f, "%.1f°")) {
//         delaunay_config.enable_min_angle_refinement = (delaunay_config.min_angle_threshold > 0.0f);
//         if (delaunay_config.enable_min_angle_refinement) {
//             delaunay_config.refine_max_steiner = std::max(delaunay_config.refine_max_steiner, 1000);
//         }
//         config_changed = true;
//     }

//     if (config_changed) {
//         send_triangulation_request(draw_info);
//     }
    
//     if (ImGui::Checkbox("Lloyd Smoothing", &delaunay_config.enable_lloyd_smoothing)) {
//         config_changed = true;
//     }
    
//     if (delaunay_config.enable_lloyd_smoothing) {
//         if (ImGui::SliderInt("Lloyd Iterations", &delaunay_config.lloyd_iterations, 1, 10)) {
//             config_changed = true;
//         }
//     }
    
//     if (ImGui::Checkbox("Edge Flipping", &delaunay_config.enable_edge_flipping)) {
//         config_changed = true;
//     }
    
//     ImGui::Separator();

//     // Drawing mode toggle
//     if (ImGui::RadioButton("Add Points", !state_.drawing_boundary)) {
//         state_.drawing_boundary = false;
//     }
//     ImGui::SameLine();
//     if (ImGui::RadioButton("Draw Boundary", state_.drawing_boundary)) {
//         state_.drawing_boundary = true;
//     }
    
//     ImGui::Separator();
    
//     // Actions
//     if (ImGui::Button("Generate Random Points")) {
//         create_random_points(50, draw_info);
//     }

//     ImGui::SameLine();

//     if (ImGui::Button("Create Grid")) {
//         create_grid_points(8, 6, draw_info);
//     }

//     if (ImGui::Button("Clear All")) {
//         mesh->reset();
//         send_triangulation_request(draw_info);
//     }

//     ImGui::SameLine();
    
//     if (ImGui::Button("Triangulate!")) {
//         send_triangulation_request(draw_info);
//     }
    
//     ImGui::Separator();
    
//     // Display options
//     ImGui::Checkbox("Show Point IDs", &state_.show_point_ids);
//     ImGui::Checkbox("Show Triangle IDs", &state_.show_triangle_ids);
//     ImGui::Checkbox("Show Edges", &state_.show_edges);
//     ImGui::Checkbox("Show BCs", &state_.show_boundary_conditions);
//     ImGui::Checkbox("Show Statistics", &state_.show_statistics);

//     ImGui::Separator();
//     ImGui::Text("Boundary Input:");
//     int bt = (int)state_.boundary_type;
//     ImGui::RadioButton("Polygon (click)", &bt, (int)BoundaryInputType::Polygon);
//     ImGui::SameLine();
//     ImGui::RadioButton("Smooth Stroke (drag)", &bt, (int)BoundaryInputType::SmoothStroke);
//     ImGui::SameLine();
//     ImGui::RadioButton("Parametric", &bt, (int)BoundaryInputType::Parametric);
//     state_.boundary_type = (BoundaryInputType)bt;
//     if (state_.boundary_type == BoundaryInputType::SmoothStroke) {
//         state_.drawing_boundary = true;
//         triangulation_session_config_.mesh_generator_config.mode = fem::DelaunayMeshGeneratorMode::WITH_BOUNDARY;
//     }
//     if (state_.boundary_type == BoundaryInputType::Parametric) {
//         state_.drawing_boundary = true;
//         triangulation_session_config_.mesh_generator_config.mode = fem::DelaunayMeshGeneratorMode::WITH_BOUNDARY;
//         draw_parametric_controls(draw_info);
//     }

//     ImGui::Separator();
//     ImGui::Text("Batch boundary assignment:");

//     ImGui::Separator();
//     ImGui::Text("Freehand:");
//     if (ImGui::Checkbox("Multi-loop (outer + holes)", &state_.multi_loop_mode)) {
//         // When toggling off multi-loop mode, we might want to keep only the first loop
//         // or clear everything - your choice
//     }
//     ImGui::SameLine();
//     if (ImGui::Button("Clear loops")) {
//         mesh->clear_boundary_loops();
//         mesh->clear_boundary_points();
//         send_triangulation_request(draw_info);
//     }

    
//     ImGui::Separator();
//     ImGui::Text("Density (sizing field)");

//     bool density_changed = false;
//     DensityConfig& density_config = triangulation_session_config_.density_config;

//     if (ImGui::Checkbox("Enable density refinement", &density_config.enable)) {
//         density_changed = true;
//     }
    
//     if (density_config.enable) {
//         density_changed |= ImGui::SliderFloat("Global h", &density_config.global_h, 5.f, 120.f, "%.1f px");
//         density_changed |= ImGui::Checkbox("Denser near boundary", &density_config.use_boundary);
//         if (density_config.use_boundary) {
//             density_changed |= ImGui::SliderFloat("Boundary h_min", &density_config.boundary_h_min, 3.f, 60.f, "%.1f");
//             density_changed |= ImGui::SliderFloat("Boundary h_max", &density_config.boundary_h_max, 5.f, 140.f, "%.1f");
//             density_changed |= ImGui::SliderFloat("Boundary influence", &density_config.boundary_influence, 5.f, 200.f, "%.1f");
//         }

//         density_changed |= ImGui::Checkbox("Radial hotspot", &density_config.use_radial);
//         if (density_config.use_radial) {
//             // density_changed |= ImGui::SliderFloat2("Hotspot center", &density_config.radial_cx, 0.f, draw_info.canvas_state->size[0]);
//             density_changed |= ImGui::SliderFloat("Inner R", &density_config.radial_r_in, 5.f, 300.f, "%.1f");
//             density_changed |= ImGui::SliderFloat("Outer R", &density_config.radial_r_out, 10.f, 400.f, "%.1f");
//             density_changed |= ImGui::SliderFloat("Hotspot h_min", &density_config.radial_h_min, 3.f, 60.f, "%.1f");
//             density_changed |= ImGui::SliderFloat("Hotspot h_max", &density_config.radial_h_max, 5.f, 140.f, "%.1f");
//         }

//         // density_changed |= ImGui::SliderInt("Max Steiner", &density_config.max_steiner, 0, 5000);
//         density_changed |= ImGui::SliderFloat("Refine when L/h >", &density_config.L_over_h_threshold, 1.05f, 2.0f, "%.2f");
//     }

//     mesh->density_config = density_config;

//     if (density_changed) {
//         send_triangulation_request(draw_info);
//     }

//     ImGui::End();
}

constexpr size_t CANVAS_OFFSET = 40ull;

void PlanarMeshGeneratorWindow::create_random_points(
    size_t point_count, 
    const PlanarMeshGeneratorWindowDrawInfo& draw_info
) {
    // float width = draw_info.canvas_state->size.x - CANVAS_OFFSET;
    // float height = draw_info.canvas_state->size.y - CANVAS_OFFSET;

    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_real_distribution<float> x_dist(20.0f, width);
    // std::uniform_real_distribution<float> y_dist(20.0f, height);
    
    // PlanarMeshComponent* mesh = draw_info.selected_mesh;
    // mesh->reset();
    
    // for (int i = 0; i < point_count; ++i) {
    //     mesh->add_user_point({ x_dist(gen), y_dist(gen) });
    // }

    // triangulation_session_config_.mesh_generator_config.mode = fem::DelaunayMeshGeneratorMode::POINTS_ONLY;

    // send_triangulation_request(draw_info);
}

void PlanarMeshGeneratorWindow::create_grid_points(
    size_t point_count_x, 
    size_t point_count_y, 
    const PlanarMeshGeneratorWindowDrawInfo& draw_info
) {
    // float width = draw_info.canvas_state->size.x - CANVAS_OFFSET;
    // float height = draw_info.canvas_state->size.y - CANVAS_OFFSET;
    
    // float dx = width  / std::max(1ull, static_cast<unsigned long long>(point_count_x - 1));
    // float dy = height / std::max(1ull, static_cast<unsigned long long>(point_count_y - 1));

    // PlanarMeshComponent* mesh = draw_info.selected_mesh;
    // mesh->reset();
    
    // for (int j = 0; j < point_count_y; ++j) {
    //     for (int i = 0; i < point_count_x; ++i) {
    //         float x = 20.0f + i * dx;
    //         float y = 20.0f + j * dy;
    //         mesh->add_user_point({ x, y });
    //     }
    // }
    
    // triangulation_session_config_.mesh_generator_config.mode = fem::DelaunayMeshGeneratorMode::POINTS_ONLY;

    // send_triangulation_request(draw_info);
}

void PlanarMeshGeneratorWindow::rebuild_density_now_if_enabled(const PlanarMeshGeneratorWindowDrawInfo& draw_info) {
    // auto& density_config  = triangulation_session_config_.density_config;
    // auto& delaunay_config = triangulation_session_config_.mesh_generator_config;
    // if (!density_config.enable) {
    //     delaunay_config.enable_sizing_refinement = density_config.enable;
    //     return;
    // }

    // const CanvasWindowState& canvas_state = *draw_info.canvas_state;
    // PlanarMeshComponent* mesh = draw_info.selected_mesh;

    // std::shared_ptr<CombinedDensity> density_combo_function = std::make_shared<CombinedDensity>();
    // density_combo_function->add_function(std::make_unique<UniformDensity>(density_config.global_h));

    // if (density_config.use_boundary) {
    //     std::vector<glm::dvec2> boundary_coords;
        
    //     // PRIORITY 1: Multi-loop mode - use outer loop from generator
    //     if (mesh->boundary_loops().size() > 0 /*&& mesh_generator->mode == DelaunayMeshGenerator::Mode::WITH_BOUNDARY*/) {
    //         LOGT_WARN(LogEditor, "Density refinment for multi loop is not implemented!");
    //         // const auto& loops = mesh_generator->boundary_loops;
    //         // if (!loops.empty()) {
    //         //     // Outer loop is first
    //         //     const auto& outer_loop = loops[0];
    //         //     boundary_coords.reserve(outer_loop.size());
    //         //     for (const auto& p : outer_loop) {
    //         //         boundary_coords.emplace_back(p.x(), p.y());
    //         //     }
                
    //         //     LOGT_DEBUG(LogEditor, "[Density] Using multi-loop outer boundary (%zu pts)", 
    //         //                  boundary_coords.size());
    //         // }
    //     }
    //     // PRIORITY 2: polygon_points (for click-based or stroke-based single loops)
    //     else if (!mesh->polygon_points().empty()) {
    //         boundary_coords.reserve(mesh->polygon_points().size());
    //         for (const auto& p : mesh->polygon_points())
    //             boundary_coords.emplace_back(p.x(), p.y());
            
    //         LOGT_DEBUG(LogEditor, "[Density] Using polygon boundary (%zu pts)", boundary_coords.size());
    //     }
    //     // PRIORITY 3: boundary_points
    //     else if (!mesh->boundary_points().empty()) {
    //         boundary_coords.reserve(mesh->boundary_points().size());
    //         for (const auto& p : mesh->boundary_points())
    //             boundary_coords.emplace_back(p.x(), p.y());
            
    //         LOGT_DEBUG(LogEditor, "[Density] Using boundary_points (%zu pts)", boundary_coords.size());
    //     }
    //     // FALLBACK: convex hull
    //     else if (!mesh->user_points().empty()) {
    //         auto hull = compute_convex_hull(mesh->user_points());
    //         boundary_coords.reserve(hull.size());
    //         for (const auto& p : hull)
    //             boundary_coords.emplace_back(p.x(), p.y());
            
    //         LOGT_DEBUG(LogEditor, "[Density] Using convex hull (%zu pts)", boundary_coords.size());
    //     }

    //     if (!boundary_coords.empty()) {
    //         density_combo_function->add_function(std::make_unique<BoundaryDensity>(
    //             boundary_coords,
    //             density_config.boundary_influence,
    //             density_config.boundary_h_min,
    //             density_config.boundary_h_max
    //         ));
    //     }
    // }

    // if (density_config.use_radial) {
    //     density_combo_function->add_function(std::make_unique<RadialDensity>(
    //         density_config.radial_center,
    //         density_config.radial_r_in,
    //         density_config.radial_r_out,
    //         density_config.radial_h_min,
    //         density_config.radial_h_max
    //     ));
        
    //     LOGT_DEBUG(LogEditor, "[Density] Radial hotspot at (%.1f, %.1f)", density_config.radial_center.x, density_config.radial_center.y);
    // }

    // triangulation_session_config_.density_function = density_combo_function;

    // delaunay_config.enable_sizing_refinement = density_config.enable;
    // delaunay_config.refine_sizing_max_steiner = std::max(1u, density_config.max_steiner);
    // delaunay_config.density_refine_threshold  = density_config.L_over_h_threshold;
}

static inline double cross(const Point2D& o, const Point2D& a, const Point2D& b) {
    return (a.x() - o.x())*(b.y() - o.y()) - (a.y() - o.y())*(b.x() - o.x());
}
static inline bool lessXY(const Point2D& a, const Point2D& b) {
    if (a.x() != b.x()) return a.x() < b.x();
    return a.y() < b.y();
}
static inline bool eqXY(const Point2D& a, const Point2D& b) {
    return std::abs(a.x() - b.x()) < 1e-12 && std::abs(a.y() - b.y()) < 1e-12;
}

std::vector<Point2D> PlanarMeshGeneratorWindow::compute_convex_hull(std::vector<Point2D> pts) {
    if (pts.size() < 3) return pts;

    // sort + dedupe
    std::sort(pts.begin(), pts.end(), lessXY);
    pts.erase(std::unique(pts.begin(), pts.end(), eqXY), pts.end());
    if (pts.size() < 3) return pts;

    std::vector<Point2D> H;
    H.reserve(pts.size()*2);

    for (const auto& p : pts) {
        while (H.size() >= 2 && cross(H[H.size()-2], H.back(), p) <= 0) H.pop_back();
        H.push_back(p);
    }
    size_t lower_sz = H.size();
    for (int i = (int)pts.size()-2; i >= 0; --i) {
        const auto& p = pts[i];
        while (H.size() > lower_sz && cross(H[H.size()-2], H.back(), p) <= 0) H.pop_back();
        H.push_back(p);
    }
    if (!H.empty()) H.pop_back();

    double area2 = 0.0;
    for (size_t i=0;i<H.size();++i) {
        const auto& a = H[i];
        const auto& b = H[(i+1)%H.size()];
        area2 += a.x()*b.y() - a.y()*b.x();
    }
    if (area2 < 0) std::reverse(H.begin(), H.end());

    for (size_t i=0;i<H.size();++i) { H[i].id = (int)i; H[i].on_boundary = true; }
    return H;
}

void PlanarMeshGeneratorWindow::send_triangulation_request(const PlanarMeshGeneratorWindowDrawInfo& draw_info) {
    rebuild_density_now_if_enabled(draw_info);
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