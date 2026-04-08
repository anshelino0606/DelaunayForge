#ifndef CANVAS_INSPECTOR
#define CANVAS_INSPECTOR

#include <glm/glm.hpp>
#include <imgui/imgui.h>

namespace fem {

struct DelaunayTriangulationResult;

class CanvasInspector final {
public:
    struct Settings {
        float vertex_pick_radius_px = 10.0f;
        float edge_pick_radius_px   = 8.0f;

        bool boundary_edges_only = false;

        bool use_tooltip = true;
    };

    enum class Kind : int {
        None = 0,
        Vertex,
        Edge,
        Triangle
    };

    struct Selection {
        Kind kind = Kind::None;
        int  id   = -1;

        bool valid() const { return kind != Kind::None && id >= 0; }
        void clear() { kind = Kind::None; id = -1; }
    };

public:
    Settings settings;

    void clear();

    bool on_right_click(
        const DelaunayTriangulationResult& R,
        const glm::dvec2& world_pos,
        const ImVec2& screen_pos,
        double viewport_zoom
    );

    void draw(const DelaunayTriangulationResult& R);

    const Selection& selection() const { return selection_; }
    bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

private:
    int pick_vertex_(const DelaunayTriangulationResult& R, const glm::dvec2& p, double r_world) const;
    int pick_edge_(const DelaunayTriangulationResult& R, const glm::dvec2& p, double r2_world) const;
    int pick_triangle_(const DelaunayTriangulationResult& R, const glm::dvec2& p) const;

    void draw_vertex_(const DelaunayTriangulationResult& R, int vid) const;
    void draw_edge_(const DelaunayTriangulationResult& R, int eid) const;
    void draw_triangle_(const DelaunayTriangulationResult& R, int tid) const;

private:
    bool visible_ = false;
    Selection selection_;

    ImVec2     anchor_screen_{0, 0};
    glm::dvec2 anchor_world_{0.0, 0.0};
};

} // namespace fem

#endif