#include "canvas_inspector.h"

#include "geom/delaunay/delaunay_types.h"
#include "geom/geometry_2d.h"

#include <algorithm>
#include <vector>

namespace fem {

void CanvasInspector::clear() {
    visible_ = false;
    selection_.clear();
    anchor_screen_ = ImVec2(0, 0);
    anchor_world_  = glm::dvec2(0.0, 0.0);
}

bool CanvasInspector::on_right_click(
    const DelaunayTriangulationResult& R,
    const glm::dvec2& world_pos,
    const ImVec2& screen_pos,
    double viewport_zoom
) {
    anchor_world_  = world_pos;
    anchor_screen_ = screen_pos;

    selection_.clear();
    visible_ = false;

    const double z = std::max(1e-6, viewport_zoom);
    const double vr_world = (double)settings.vertex_pick_radius_px / z;
    const double er_world = (double)settings.edge_pick_radius_px   / z;

    const int v = pick_vertex_(R, world_pos, vr_world);
    if (v >= 0) {
        selection_ = { Kind::Vertex, v };
        visible_ = true;
        return true;
    }

    const int e = pick_edge_(R, world_pos, er_world * er_world);
    if (e >= 0) {
        selection_ = { Kind::Edge, e };
        visible_ = true;
        return true;
    }

    const int t = pick_triangle_(R, world_pos);
    if (t >= 0) {
        selection_ = { Kind::Triangle, t };
        visible_ = true;
        return true;
    }

    return false;
}

int CanvasInspector::pick_vertex_(const DelaunayTriangulationResult& R, const glm::dvec2& p, double r_world) const {
    const double r2 = r_world * r_world;
    int best = -1;
    double best_d2 = r2;

    for (int i = 0; i < (int)R.points.size(); ++i) {
        const double d2 = Geometry2D::dist2(p, R.points[i].p);
        if (d2 <= best_d2) {
            best_d2 = d2;
            best = i;
        }
    }
    return best;
}

int CanvasInspector::pick_edge_(const DelaunayTriangulationResult& R, const glm::dvec2& p, double r2_world) const {
    int best = -1;
    double best_d2 = r2_world;

    for (int i = 0; i < (int)R.edges.size(); ++i) {
        const auto& E = R.edges[i];
        if (settings.boundary_edges_only && !E.on_boundary) continue;

        const Point2D& a = R.points[E.a];
        const Point2D& b = R.points[E.b];

        const double d2 = Geometry2D::point_segment_dist2(p, a, b);
        if (d2 <= best_d2) {
            best_d2 = d2;
            best = i;
        }
    }
    return best;
}

int CanvasInspector::pick_triangle_(const DelaunayTriangulationResult& R, const glm::dvec2& p) const {
    for (int i = 0; i < (int)R.triangles.size(); ++i) {
        const auto& T = R.triangles[i];
        if (!T.valid) continue;

        const Point2D& a = R.points[T.v[0]];
        const Point2D& b = R.points[T.v[1]];
        const Point2D& c = R.points[T.v[2]];

        if (Geometry2D::point_in_triangle(p, a, b, c))
            return i;
    }
    return -1;
}

void CanvasInspector::draw(const DelaunayTriangulationResult& R) {
    if (!visible_ || !selection_.valid()) return;

    ImGui::SetNextWindowPos(anchor_screen_, ImGuiCond_Always, ImVec2(0.f, 0.f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing;

    bool open = true;

    if (settings.use_tooltip) {
        ImGui::BeginTooltip();
    } else {
        ImGui::Begin("Inspector", &open, flags);
    }

    ImGui::Text("Pick @ (%.3f, %.3f)", anchor_world_.x, anchor_world_.y);
    ImGui::Separator();

    switch (selection_.kind) {
        case Kind::Vertex:   draw_vertex_(R, selection_.id); break;
        case Kind::Edge:     draw_edge_(R, selection_.id); break;
        case Kind::Triangle: draw_triangle_(R, selection_.id); break;
        default: break;
    }

    if (settings.use_tooltip) {
        ImGui::EndTooltip();
    } else {
        ImGui::End();
        if (!open) visible_ = false;
    }
}

void CanvasInspector::draw_vertex_(const DelaunayTriangulationResult& R, int vid) const {
    if (vid < 0 || (size_t)vid >= R.points.size()) return;

    const auto& P = R.points[vid];

    ImGui::Text("Vertex: %d", vid);
    ImGui::Text("Pos: (%.6f, %.6f)", (double)P.x(), (double)P.y());
    ImGui::Text("on_boundary: %s", P.on_boundary ? "true" : "false");

    ImGui::Separator();
    ImGui::Text("Incident triangles:");

    int count = 0;
    if (!R.vert2tri.empty() && (size_t)vid < R.vert2tri.size()) {
        count = (int)R.vert2tri[vid].size();
        if (count == 0) {
            ImGui::TextDisabled("none");
        } else {
            for (int tid : R.vert2tri[vid]) ImGui::Text("%d", tid);
        }
    } else {
        // Fallback scan
        for (int i = 0; i < (int)R.triangles.size(); ++i) {
            const auto& T = R.triangles[i];
            if (!T.valid) continue;
            if (T.v[0] == vid || T.v[1] == vid || T.v[2] == vid) {
                ImGui::Text("%d", i);
                ++count;
            }
        }
        if (count == 0) ImGui::TextDisabled("none");
    }

    ImGui::Separator();
    ImGui::Text("Valence: %d", count);
}

void CanvasInspector::draw_edge_(const DelaunayTriangulationResult& R, int eid) const {
    if (eid < 0 || (size_t)eid >= R.edges.size()) return;

    const EdgeInfo& E = R.edges[eid];
    const Point2D& A = R.points[E.a];
    const Point2D& B = R.points[E.b];

    const double L = glm::distance(A.p, B.p);

    ImGui::Text("Edge: %d", eid);
    ImGui::Text("Endpoints: (%d, %d)", E.a, E.b);
    ImGui::Text("A: (%.6f, %.6f)", (double)A.x(), (double)A.y());
    ImGui::Text("B: (%.6f, %.6f)", (double)B.x(), (double)B.y());
    ImGui::Text("Length: %.6f", L);
    ImGui::Text("on_boundary: %s", E.on_boundary ? "true" : "false");

    // Optional: find adjacent triangles by scan (cheap enough for inspector clicks)
    ImGui::Separator();
    ImGui::Text("Adjacent triangles:");
    int found = 0;
    for (int i = 0; i < (int)R.triangles.size(); ++i) {
        const auto& T = R.triangles[i];
        if (!T.valid) continue;
        const int v0 = T.v[0], v1 = T.v[1], v2 = T.v[2];

        auto has = [&](int v) { return v0 == v || v1 == v || v2 == v; };
        if (has(E.a) && has(E.b)) {
            ImGui::Text("%d", i);
            ++found;
        }
    }
    if (found == 0) ImGui::TextDisabled("none");
}

void CanvasInspector::draw_triangle_(const DelaunayTriangulationResult& R, int tid) const {
    if (tid < 0 || (size_t)tid >= R.triangles.size()) return;

    const auto& T = R.triangles[tid];
    if (!T.valid) {
        ImGui::Text("Triangle: %d (invalid)", tid);
        return;
    }

    ImGui::Text("Triangle: %d", tid);
    ImGui::Text("Verts: (%d, %d, %d)", T.v[0], T.v[1], T.v[2]);

    const Point2D& a = R.points[T.v[0]];
    const Point2D& b = R.points[T.v[1]];
    const Point2D& c = R.points[T.v[2]];

    ImGui::Text("Area: %.6f", Geometry2D::tri_area(a, b, c));
    ImGui::Text("Min angle: %.3f deg", Geometry2D::min_angle_deg(a, b, c));

    ImGui::Separator();
    if (!R.tri_neighbors.empty() && (size_t)tid < R.tri_neighbors.size()) {
        const auto& N = R.tri_neighbors[tid];
        ImGui::Text("Neighbors: (%d, %d, %d)", N[0], N[1], N[2]);
    } else {
        ImGui::TextDisabled("Neighbors: n/a");
    }
}

} // namespace fem
