#include "canvas_inspector.h"

#include "geom/delaunay_types.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fem {

using PointT =
    std::remove_cv_t<std::remove_reference_t<
        decltype(std::declval<DelaunayTriangulationResult>().points[0])
    >>;

static inline glm::dvec2 p2(const PointT& P) {
    return glm::dvec2((double)P.x(), (double)P.y());
}

static inline glm::dvec2 p2(const decltype(DelaunayTriangulationResult{}.points[0])& P) {
    return glm::dvec2((double)P.x(), (double)P.y());
}

static inline double dist2(const glm::dvec2& a, const glm::dvec2& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx*dx + dy*dy;
}

static inline double point_segment_dist2(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b) {
    const glm::dvec2 ab = b - a;
    const glm::dvec2 ap = p - a;
    const double ab2 = ab.x*ab.x + ab.y*ab.y;
    if (ab2 <= 1e-30) return dist2(p, a);

    double t = (ap.x*ab.x + ap.y*ab.y) / ab2;
    t = std::clamp(t, 0.0, 1.0);
    const glm::dvec2 q = a + t * ab;
    return dist2(p, q);
}

static inline double cross2(const glm::dvec2& a, const glm::dvec2& b) {
    return a.x*b.y - a.y*b.x;
}

static inline bool point_in_triangle(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    const glm::dvec2 ab = b - a, bc = c - b, ca = a - c;
    const glm::dvec2 ap = p - a, bp = p - b, cp = p - c;

    const double c1 = cross2(ab, ap);
    const double c2 = cross2(bc, bp);
    const double c3 = cross2(ca, cp);

    const bool has_neg = (c1 < 0.0) || (c2 < 0.0) || (c3 < 0.0);
    const bool has_pos = (c1 > 0.0) || (c2 > 0.0) || (c3 > 0.0);
    return !(has_neg && has_pos);
}

static inline double tri_area_abs(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    return 0.5 * std::abs(cross2(b - a, c - a));
}

static inline double safe_acos(double x) {
    return std::acos(std::clamp(x, -1.0, 1.0));
}

static inline double min_angle_deg(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    auto angle = [](const glm::dvec2& u, const glm::dvec2& v) -> double {
        const double nu = std::sqrt(u.x*u.x + u.y*u.y);
        const double nv = std::sqrt(v.x*v.x + v.y*v.y);
        if (nu <= 1e-30 || nv <= 1e-30) return 0.0;
        const double cs = (u.x*v.x + u.y*v.y) / (nu * nv);
        return safe_acos(cs);
    };

    const double A = angle(b - a, c - a);
    const double B = angle(a - b, c - b);
    const double C = angle(a - c, b - c);

    const double m = std::min(A, std::min(B, C));
    return m * 180.0 / M_PI;
}

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
        const glm::dvec2 q = p2(R.points[i]);
        const double d2 = dist2(p, q);
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

        const glm::dvec2 a = p2(R.points[E.a]);
        const glm::dvec2 b = p2(R.points[E.b]);

        const double d2 = point_segment_dist2(p, a, b);
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

        const glm::dvec2 a = p2(R.points[T.v[0]]);
        const glm::dvec2 b = p2(R.points[T.v[1]]);
        const glm::dvec2 c = p2(R.points[T.v[2]]);

        if (point_in_triangle(p, a, b, c))
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

    const auto& E = R.edges[eid];
    const auto& A = R.points[E.a];
    const auto& B = R.points[E.b];

    const glm::dvec2 a = p2(A);
    const glm::dvec2 b = p2(B);
    const double L = std::sqrt(dist2(a, b));

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

    const glm::dvec2 a = p2(R.points[T.v[0]]);
    const glm::dvec2 b = p2(R.points[T.v[1]]);
    const glm::dvec2 c = p2(R.points[T.v[2]]);

    ImGui::Text("Area: %.6f", tri_area_abs(a, b, c));
    ImGui::Text("Min angle: %.3f deg", min_angle_deg(a, b, c));

    ImGui::Separator();
    if (!R.tri_neighbors.empty() && (size_t)tid < R.tri_neighbors.size()) {
        const auto& N = R.tri_neighbors[tid];
        ImGui::Text("Neighbors: (%d, %d, %d)", N[0], N[1], N[2]);
    } else {
        ImGui::TextDisabled("Neighbors: n/a");
    }
}

} // namespace fem
