#ifndef FEM_EDITOR_H
#define FEM_EDITOR_H


#include "outliner_window.h"
#include "docking_window.h"
#include "canvas_window.h"
#include "details_window.h"
#include "viewport_3d_window.h"
#include "main_menu_toolbar.h"
#include "planar_mesh_generator_window.h"
#include <glm/glm.hpp>
#include <vector>
#include "triangulation_session.h"
#include "mesh_element_info_window.h"
#include "tools/canvas_inspector.h"
#include "tools/fem_error_analysis_window.h"
#include "renderer/viewport_grid_settings.h"

namespace fem {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
enum class TriBackend { CPU, GPU };

class Window;
class Entity;class Camera;
class Renderer;
using DrawDebugInfoCallback = std::function<void()>;

struct EditorInitInfo {
    Window* window = nullptr;
    DrawDebugInfoCallback draw_debug_info_callback;
};

struct EditorDrawInfo {
    const std::vector<Entity*>* entities;
    ImTextureID viewport_texture_id = 0;
};

struct EditorDrawResult {
    glm::uvec2 canvas_pos = glm::uvec2(0);
    glm::uvec2 canvas_size = glm::uvec2(0);
    glm::uvec2 scissor_pos = glm::uvec2(0);
    glm::uvec2 scissor_size = glm::uvec2(0);

    glm::uvec2 viewport_pos = glm::uvec2(0);
    glm::uvec2 viewport_size = glm::uvec2(0);
    bool is_viewport_active = false;

    ViewportGridSettings viewport_grid_settings;
    const Viewport3DCaptureSettings* viewport_capture_settings;

    Entity* selected_entity = nullptr;
};

class Editor {
public:
    bool init(const EditorInitInfo& init_info);
    void reset();
    void shutdown();

    EditorDrawResult draw(const EditorDrawInfo& draw_info);

    // TEMP
    const TriangulationSession& session() const { return session_; }

    const ViewportGridSettings& viewport_grid_settings() const { return viewport_grid_settings_; }

private:
    Window* window_ = nullptr;
    bool is_initialized_ = false;

    OutlinerWindow outliner_;
    DockingWindow docking_window_;
    CanvasWindow canvas_;
    Viewport3DWindow viewport_;
    DetailsWindow details_;
    MainMenuToolbar main_menu_toolbar_;
    MeshElementInfoWindow mesh_info_;
    FEMErrorAnalysisWindow fem_error_;
    PlanarMeshGeneratorWindow planar_mesh_generator_window_;

    TriBackend backend_ = TriBackend::CPU;

    TriangulationSession session_; 
    DrawDebugInfoCallback draw_debug_info_callback_;

    bool request_canvas_export_popup_ = false;
    bool request_viewport_capture_popup_ = false;

    ViewportGridSettings viewport_grid_settings_;

    struct PickState {
        PlanarMeshComponent* mesh = nullptr;
        CanvasInspector::Selection sel;
        glm::dvec2 world_pos{0.0, 0.0};
    } pick_;
};

}

#endif // FEM_EDITOR_H