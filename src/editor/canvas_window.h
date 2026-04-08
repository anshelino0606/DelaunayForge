#ifndef FEM_CANVAS_H
#define FEM_CANVAS_H

#include "viewport.h"
#include "smooth_stroke_tool.h"
#include "tools/canvas_inspector.h"

#include <string>
#include <unordered_set>
#include <glm/glm.hpp>

struct DelaunayTriangulationResult;

namespace fem {

class Entity;
class PDEComponent;
class PlanarMeshComponent;
class PlanarMeshBoundaryBase;
struct PlanarTriangulationSessionConfig;
struct PlanarMeshGeneratorWindowState;
class BoundaryCondition;
class BCGroupManager;

struct CanvasWindowDrawInfo {
    Entity* selected_entity = nullptr;
    PlanarMeshComponent* selected_mesh = nullptr;
    PDEComponent* selected_pde = nullptr;
    BCGroupManager* bc_group_manager = nullptr;  // NEW: for managing boundary groups
    const PlanarMeshGeneratorWindowState* mesh_generator_state = nullptr;
    const PlanarTriangulationSessionConfig* triangulation_session_config = nullptr;
    std::function<void(const std::vector<Point2D>&)> on_stroke_loop_committed;
    std::function<void(PlanarMeshComponent*,
                       const CanvasInspector::Selection&,
                       const glm::dvec2&)> on_inspector_pick;
};

struct CanvasWindowState {
    static constexpr int32_t s_invalid = -1;

    glm::vec2 size = glm::vec2(0);
    glm::vec2 position = glm::vec2(0);
};

class CanvasWindow {
public:
    void draw(const CanvasWindowDrawInfo& draw_info);

    void request_export_popup() { export_popup_requested_ = true; }

    CanvasWindowState& state() { return state_; }
    const CanvasWindowState& state() const { return state_; }

private:
    Viewport viewport_;
    CanvasWindowState state_;
    CanvasInspector inspector_;

    SmoothStrokeTool stroke_tool_;

    float point_radius_ = 2.0f;
    float  vertex_pick_radius_px_ = 8.0f;
    float  edge_pick_radius_px_   = 10.0f;

    struct ExportSettings {
        enum class Theme : uint8_t { Dark, Light };

        Theme theme = Theme::Light;
        int scale_factor = 2;           // export resolution = canvas_size * scale_factor
        bool include_axes = true;
        bool include_solution = true;
        bool include_mesh = true;
        bool include_points = true;
        bool include_colorbar = true;   // only applies when PDE solution bounds are available
        bool include_boundary_conditions = true;
        bool include_bc_legend = true;  // show legend for boundary conditions

        int format_index = 0; // 0 = PNG, 1 = SVG
    } export_settings_;

    bool export_popup_requested_ = false;
    bool export_popup_open_ = false;
    std::string export_path_;

    Entity* last_entity_ = nullptr;
    PlanarMeshComponent* last_mesh_ = nullptr;
    PDEComponent* last_pde_ = nullptr;

    enum class SelectionMode {
        None,
        Single,        // Default click selection
        Multi,         // Ctrl+Click for multi-select
        RubberBand     // Drag to select multiple edges
    };

    SelectionMode selection_mode_ = SelectionMode::Single;
    bool is_rubber_band_active_ = false;
    glm::vec2 rubber_band_start_;  // Screen coordinates
    glm::vec2 rubber_band_end_;    // Screen coordinates
    std::unordered_set<int> selected_edge_ids_;
    std::unordered_set<int> hovered_edge_ids_;

    void draw_export_popup();
    bool export_svg(const std::string& absolute_path) const;
    bool export_png(const std::string& absolute_path) const;

    void on_click(PlanarMeshBoundaryBase* selected_boundary);
    void on_right_click(const CanvasWindowDrawInfo& draw_info);

    void draw_solution(const CanvasWindowDrawInfo& draw_info);
    void draw_mesh(PlanarMeshComponent* mesh);
    void draw_selection_overlay(const CanvasWindowDrawInfo& draw_info);
    void draw_rubber_band();

    int pick_vertex(const DelaunayTriangulationResult& R, const glm::dvec2& world_pos, double r_px);
    int pick_edge(const DelaunayTriangulationResult& R, const glm::dvec2& world_pos, double max_d2);
    std::vector<int> pick_edges_in_rect(const DelaunayTriangulationResult& R, 
                                        const glm::vec2& screen_min, 
                                        const glm::vec2& screen_max);

    void start_rubber_band_selection(const glm::vec2& screen_pos);
    void update_rubber_band_selection(const glm::vec2& screen_pos);
    void finish_rubber_band_selection(const CanvasWindowDrawInfo& draw_info);
    void cancel_rubber_band_selection();

    void clear_edge_selection();
    void add_edge_to_selection(int edge_id);
    void remove_edge_from_selection(int edge_id);
    void toggle_edge_selection(int edge_id);
    bool is_edge_selected(int edge_id) const;

    void request_triangulation(const PlanarTriangulationSessionConfig& config);

    bool is_draw_info_valid(const CanvasWindowDrawInfo& draw_info) const;
};

}

#endif // FEM_CANVAS_H