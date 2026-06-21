#ifndef FEM_MESH_GENERATOR_H
#define FEM_MESH_GENERATOR_H

#include "geom/triangulation_session_config.h"
#include "smooth_stroke_tool.h"
#include "tools/parametric_curve.h"

namespace fem {

class PlanarMeshComponent;
struct CanvasWindowState;

struct PlanarMeshGeneratorWindowDrawInfo {
    PlanarMeshComponent* selected_mesh = nullptr;
    CanvasWindowState* canvas_state = nullptr; 
};

// enum class BoundaryInputType { 
//     Polygon, 
//     SmoothStroke, 
//     Parametric 
// };

struct PlanarMeshGeneratorWindowState {
    bool drawing_boundary = false;
    bool show_point_ids = false;
    bool show_triangle_ids = false;
    bool show_edges = true;
    bool show_boundary_conditions = true;
    bool show_statistics = false;
    bool multi_loop_mode = false;

    // BoundaryInputType boundary_type = BoundaryInputType::Polygon;

    SmoothStrokeConfig stroke_cfg;
    ParametricCurveConfig parametric_cfg;
};

class PlanarMeshGeneratorWindow {
public:
    PlanarMeshGeneratorWindow();

    void draw(const PlanarMeshGeneratorWindowDrawInfo& draw_info);

    const PlanarTriangulationSessionConfig& triangulation_session_config() const {
        return triangulation_session_config_;
    }

    const PlanarMeshGeneratorWindowState& state() const { return state_; }

private:
    PlanarMeshGeneratorWindowState state_;
    PlanarTriangulationSessionConfig triangulation_session_config_;\
    ParametricCurveTool parametric_tool_;

    void send_triangulation_request(const PlanarMeshGeneratorWindowDrawInfo& draw_info);

    void draw_parametric_controls(const PlanarMeshGeneratorWindowDrawInfo& draw_info);
    void generate_parametric_boundary(const PlanarMeshGeneratorWindowDrawInfo& draw_info);
};

}

#endif // FEM_MESH_GENERATOR_H