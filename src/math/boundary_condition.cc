#include "boundary_condition.h"

#include "geom/delaunay_types.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "log_categories.h"
#include "math/fem/bc_value.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>

namespace fem {

template <class EnumLike>
static inline uint32_t to_u32(const EnumLike& e) {
    return static_cast<uint32_t>(static_cast<const uint32_t&>(e));
}

FEM_DEFINE_ENUM(BoundaryConditionType, DrawAsToggles(true));
FEM_DEFINE_ENUM(BoundaryConditionPathMode, DrawAsToggles(true));

#define SHOW_WHEN_SELECTED() \
    SHOW_WHEN_MEMBER(BoundaryCondition, is_selected_, val)

FEM_DEFINE_OBJECT(BoundaryCondition, Object, DrawCallbacks());
FEM_BEGIN_PROPERTY_REGISTER(BoundaryCondition)
{
    FEM_REGISTER_PROPERTY(
        BoundaryCondition,
        value_,
        SHOW_FOR_ENUM(type_,
                      BoundaryConditionType::Dirichlet,
                      BoundaryConditionType::Neumann,
                      BoundaryConditionType::Robin),
        ClampMin(-10),
        ClampMax(10),
        DragSpeed(0.01f),
        Format("%.2f"));

    FEM_REGISTER_PROPERTY(
        BoundaryCondition,
        value_beta_,
        SHOW_FOR_ENUM(type_, BoundaryConditionType::Robin),
        ClampMin(-10),
        ClampMax(10),
        DragSpeed(0.01f),
        Format("%.2f"));

    FEM_REGISTER_PROPERTY(BoundaryCondition, type_);
    FEM_REGISTER_PROPERTY(BoundaryCondition,
                          path_mode_,
                          ON_VALUE_CHANGED(BoundaryCondition, rebuild));

    FEM_REGISTER_FUNCTION(BoundaryCondition,
                          begin_selection,
                          DisplayName("Select New Edges"),
                          SHOW_WHEN_MEMBER(BoundaryCondition, is_selected_, !val));

    FEM_REGISTER_FUNCTION(BoundaryCondition,
                          apply_selection,
                          DisplayName("Apply"),
                          SHOW_WHEN_SELECTED());

    FEM_REGISTER_FUNCTION(BoundaryCondition,
                          switch_path_mode_to_alternative,
                          SameLine(),
                          DisplayName("Alternate Path"));

    FEM_REGISTER_FUNCTION(BoundaryCondition,
                          clear_selection,
                          SameLine(),
                          DisplayName("Clear"),
                          SHOW_WHEN_SELECTED());

    FEM_REGISTER_FUNCTION(BoundaryCondition,
                          cancel_selection,
                          SameLine(),
                          DisplayName("Cancel"),
                          SHOW_WHEN_SELECTED());

    FEM_REGISTER_PROPERTY(BoundaryCondition, edge_ids_, NoUI());
}
FEM_END_PROPERTY_REGISTER(BoundaryCondition);

namespace {

struct ProjRes {
    double s = 0.0;      // normalized [0,1)
    double dist2 = 0.0;  // squared distance to loop polyline
};

static inline double dist2_point_segment(const glm::dvec2& P,
                                         const glm::dvec2& A,
                                         const glm::dvec2& B) {
    const glm::dvec2 AB = B - A;
    const glm::dvec2 AP = P - A;
    const double ab2 = AB.x * AB.x + AB.y * AB.y;
    if (ab2 <= 0.0) {
        const double dx = P.x - A.x;
        const double dy = P.y - A.y;
        return dx * dx + dy * dy;
    }

    const double t = std::clamp((AP.x * AB.x + AP.y * AB.y) / ab2, 0.0, 1.0);
    const glm::dvec2 Q = A + t * AB;
    const double dx = P.x - Q.x;
    const double dy = P.y - Q.y;
    return dx * dx + dy * dy;
}

static inline double wrap_delta_s(double a, double b) {
    const double d = std::abs(a - b);
    return std::min(d, 1.0 - d);
}

static ProjRes project_point_to_loop_s(const std::vector<Point2D>& loop,
                                       const glm::dvec2& P,
                                       double& out_total_len) {
    ProjRes out;
    out.dist2 = std::numeric_limits<double>::infinity();
    out.s = 0.0;
    out_total_len = 0.0;

    if (loop.size() < 2) return out;

    double best_abs = 0.0;
    double prefix = 0.0;

    for (std::size_t i = 0; i < loop.size(); ++i) {
        const glm::dvec2 A(loop[i].x(), loop[i].y());
        const glm::dvec2 B(loop[(i + 1) % loop.size()].x(),
                           loop[(i + 1) % loop.size()].y());
        const glm::dvec2 AB = B - A;
        const double len = std::hypot(AB.x, AB.y);
        if (len <= 0.0) continue;

        const glm::dvec2 AP = P - A;
        const double t = std::clamp((AP.x * AB.x + AP.y * AB.y) / (len * len),
                                    0.0,
                                    1.0);

        const glm::dvec2 Q = A + t * AB;
        const double dx = P.x - Q.x;
        const double dy = P.y - Q.y;
        const double d2 = dx * dx + dy * dy;

        if (d2 < out.dist2) {
            out.dist2 = d2;
            best_abs = prefix + t * len;
        }

        prefix += len;
    }

    out_total_len = prefix;
    if (out_total_len > 0.0) {
        out.s = best_abs / out_total_len;
        out.s -= std::floor(out.s);
        if (out.s < 0.0) out.s += 1.0;
    }

    return out;
}

static glm::dvec2 point_at_s(const std::vector<Point2D>& loop,
                             double total_len,
                             double s,
                             std::size_t& out_seg) {
    out_seg = 0;
    if (loop.size() < 2 || total_len <= 0.0) {
        return loop.empty() ? glm::dvec2(0.0)
                            : glm::dvec2(loop.front().x(), loop.front().y());
    }

    s -= std::floor(s);
    if (s < 0.0) s += 1.0;

    const double target = s * total_len;
    double prefix = 0.0;

    for (std::size_t i = 0; i < loop.size(); ++i) {
        const glm::dvec2 A(loop[i].x(), loop[i].y());
        const glm::dvec2 B(loop[(i + 1) % loop.size()].x(),
                           loop[(i + 1) % loop.size()].y());
        const glm::dvec2 AB = B - A;
        const double len = std::hypot(AB.x, AB.y);
        if (len <= 0.0) continue;

        if (prefix + len >= target) {
            const double t = std::clamp((target - prefix) / len, 0.0, 1.0);
            out_seg = i;
            return A + t * AB;
        }

        prefix += len;
    }

    out_seg = 0;
    return glm::dvec2(loop.front().x(), loop.front().y());
}

static bool compute_chain_endpoints(const DelaunayTriangulationResult& R,
                                    const std::vector<int>& edge_ids,
                                    int& out_start,
                                    int& out_end) {
    if (edge_ids.empty()) return false;
    const int e0 = edge_ids.front();
    if ((unsigned)e0 >= (unsigned)R.edges.size()) return false;

    out_start = R.edges[e0].a;
    out_end = R.edges[e0].b;

    for (std::size_t i = 1; i < edge_ids.size(); ++i) {
        const int eid = edge_ids[i];
        if ((unsigned)eid >= (unsigned)R.edges.size()) continue;
        const auto& e = R.edges[eid];

        if (e.a == out_end) {
            out_end = e.b;
        } else if (e.b == out_end) {
            out_end = e.a;
        } else if (e.a == out_start) {
            out_start = e.b;
        } else if (e.b == out_start) {
            out_start = e.a;
        } else {
            return false;
        }
    }

    return true;
}

static bool compute_loop_diagnostics(const std::vector<Point2D>& loop,
                                     double& out_total_len,
                                     double& out_diag) {
    out_total_len = 0.0;
    out_diag = 0.0;
    if (loop.size() < 2) return false;

    double minx = loop.front().x();
    double miny = loop.front().y();
    double maxx = minx;
    double maxy = miny;

    for (std::size_t i = 0; i < loop.size(); ++i) {
        minx = std::min(minx, loop[i].x());
        miny = std::min(miny, loop[i].y());
        maxx = std::max(maxx, loop[i].x());
        maxy = std::max(maxy, loop[i].y());

        const glm::dvec2 A(loop[i].x(), loop[i].y());
        const glm::dvec2 B(loop[(i + 1) % loop.size()].x(),
                           loop[(i + 1) % loop.size()].y());
        out_total_len += std::hypot(B.x - A.x, B.y - A.y);
    }

    out_diag = std::hypot(maxx - minx, maxy - miny);
    return out_total_len > 0.0;
}

static bool pick_boundary_vertex_by_param(const DelaunayTriangulationResult& R,
                                          const std::vector<Point2D>& loop,
                                          double loop_total_len,
                                          double loop_diag,
                                          double target_s,
                                          int& out_vid) {
    out_vid = -1;
    if (loop.size() < 2 || loop_total_len <= 0.0) return false;

    const double eps = std::max(1e-6, 1e-8 * std::max(1.0, loop_diag));
    const double eps2 = eps * eps;

    std::size_t seg = 0;
    const glm::dvec2 target = point_at_s(loop, loop_total_len, target_s, seg);

    int best = -1;
    double best_d2 = std::numeric_limits<double>::infinity();
    for (int i = 0; i < (int)R.points.size(); ++i) {
        if (!R.points[i].on_boundary) continue;
        const auto& P = R.points[i].p;
        const double dx = P.x - target.x;
        const double dy = P.y - target.y;
        const double d2 = dx * dx + dy * dy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best = i;
        }
    }

    if (best < 0) return false;

    const auto& Pbest = R.points[best].p;
    const std::size_t n = loop.size();
    const std::size_t seg_prev = (seg + n - 1) % n;
    const std::size_t seg_next = (seg + 1) % n;

    const glm::dvec2 segA(loop[seg].x(), loop[seg].y());
    const glm::dvec2 segB(loop[(seg + 1) % n].x(), loop[(seg + 1) % n].y());
    const glm::dvec2 prevA(loop[seg_prev].x(), loop[seg_prev].y());
    const glm::dvec2 prevB(loop[(seg_prev + 1) % n].x(),
                           loop[(seg_prev + 1) % n].y());
    const glm::dvec2 nextA(loop[seg_next].x(), loop[seg_next].y());
    const glm::dvec2 nextB(loop[(seg_next + 1) % n].x(),
                           loop[(seg_next + 1) % n].y());

    const double d2_seg = dist2_point_segment(Pbest, segA, segB);
    const double d2_prev = dist2_point_segment(Pbest, prevA, prevB);
    const double d2_next = dist2_point_segment(Pbest, nextA, nextB);
    const double d2_local = std::min(d2_seg, std::min(d2_prev, d2_next));
    if (d2_local <= eps2) {
        out_vid = best;
        return true;
    }

    int best_on_loop = -1;
    double best_on_loop_ds = std::numeric_limits<double>::infinity();
    double best_on_loop_d2 = std::numeric_limits<double>::infinity();

    int best_any = -1;
    double best_any_d2 = std::numeric_limits<double>::infinity();

    for (int i = 0; i < (int)R.points.size(); ++i) {
        if (!R.points[i].on_boundary) continue;
        const auto& P = R.points[i].p;
        double total_len = 0.0;
        const ProjRes pr = project_point_to_loop_s(loop, P, total_len);

        if (pr.dist2 < best_any_d2) {
            best_any_d2 = pr.dist2;
            best_any = i;
        }

        if (pr.dist2 <= eps2) {
            const double ds = wrap_delta_s(pr.s, target_s);
            if (ds < best_on_loop_ds ||
                (ds == best_on_loop_ds && pr.dist2 < best_on_loop_d2)) {
                best_on_loop_ds = ds;
                best_on_loop_d2 = pr.dist2;
                best_on_loop = i;
            }
        }
    }

    if (best_on_loop >= 0) {
        out_vid = best_on_loop;
        return true;
    }
    if (best_any >= 0) {
        out_vid = best_any;
        return true;
    }
    return false;
}

} // namespace

void BoundaryCondition::apply(DelaunayTriangulationResult& R) const {
    fem::BoundaryValue v = fem::BoundaryValue::none();

    const uint32_t t = to_u32(type_);
    if (t == to_u32(BoundaryConditionType::Dirichlet)) {
        v = fem::BoundaryValue::dirichlet(value_);
    } else if (t == to_u32(BoundaryConditionType::Neumann)) {
        v = fem::BoundaryValue::neumann(value_);
    } else if (t == to_u32(BoundaryConditionType::Robin)) {
        v = fem::BoundaryValue::robin(value_, value_beta_);
    } else {
        v = fem::BoundaryValue::none();
    }

    for (int eid : edge_ids_) {
        if ((unsigned)eid >= (unsigned)R.edges.size()) continue;
        auto& e = R.edges[eid];
        if (!e.on_boundary) continue;
        e.bc = v;
    }
}

void BoundaryCondition::reset() {
    edge_ids_.clear();
    start_point_ = s_invalid;
    end_point_ = s_invalid;
    loop_index_ = s_invalid;
    has_param_ = false;
    has_geometry_ = false;
    arc_positions_.clear();
}

void BoundaryCondition::set_edge_ids(const std::vector<int>& edge_ids) {
    edge_ids_ = edge_ids;

    has_geometry_ = false;
    arc_positions_.clear();
    has_param_ = false;
    loop_index_ = s_invalid;

    capture_geometry_from_edges();
}

void BoundaryCondition::set_start_point(int start_point) {
    start_point_ = start_point;
    has_geometry_ = false;
    has_param_ = false;
}

void BoundaryCondition::set_end_point(int end_point) {
    end_point_ = end_point;
    has_geometry_ = false;
    has_param_ = false;
    rebuild();
}

void BoundaryCondition::switch_path_mode_to_alternative() {
    constexpr BoundaryConditionPathMode alternatives[] = {BoundaryConditionPathMode::Longer,
                                                         BoundaryConditionPathMode::Shorter,
                                                         BoundaryConditionPathMode::CCW,
                                                         BoundaryConditionPathMode::CW};

    path_mode_ = alternatives[static_cast<size_t>(path_mode_)];
    rebuild();
}

std::vector<int> BoundaryCondition::compute_boundary_arc_edges(const DelaunayTriangulationResult& R,
                                                               int v0,
                                                               int v1,
                                                               BoundaryConditionPathMode mode) {
    std::vector<int> vs, eids;
    if (!extract_boundary_loop(R, v0, vs, eids)) return {};

    auto arc_fw = arc_between_on_loop(vs, eids, v0, v1, /*forward=*/true);
    auto arc_bw = arc_between_on_loop(vs, eids, v0, v1, /*forward=*/false);

    if (arc_fw.empty() || arc_bw.empty()) {
        return find_boundary_path_edge_ids(R, v0, v1);
    }

    const double A = polygon_area_sign(R, vs);

    switch (mode) {
        case BoundaryConditionPathMode::Shorter:
            return (arc_fw.size() <= arc_bw.size()) ? arc_fw : arc_bw;
        case BoundaryConditionPathMode::Longer:
            return (arc_fw.size() >= arc_bw.size()) ? arc_fw : arc_bw;
        case BoundaryConditionPathMode::CW:
            return (A < 0.0) ? arc_fw : arc_bw;
        case BoundaryConditionPathMode::CCW:
            return (A > 0.0) ? arc_fw : arc_bw;
        default:
            return (arc_fw.size() <= arc_bw.size()) ? arc_fw : arc_bw;
    }
}

bool BoundaryCondition::extract_boundary_loop(const DelaunayTriangulationResult& R,
                                             int v_start,
                                             std::vector<int>& loop_vs,
                                             std::vector<int>& loop_eids) {
    auto G = build_boundary_graph(R);
    if (!G.count(v_start) || G[v_start].size() < 1) return false;

    loop_vs.clear();
    loop_eids.clear();

    int cur = v_start;
    int prev = -1;
    const int guard_max = (int)R.edges.size() + 5;
    int guard = 0;

    do {
        if (++guard > guard_max) return false;
        loop_vs.push_back(cur);

        const auto& nbrs = G[cur];
        int next = -1;
        int via_eid = -1;

        if (nbrs.size() == 1) {
            return false;
        }

        auto [n0, e0] = nbrs[0];
        auto [n1, e1] = nbrs[1];
        if (prev == -1) {
            next = n0;
            via_eid = e0;
        } else if (n0 != prev) {
            next = n0;
            via_eid = e0;
        } else {
            next = n1;
            via_eid = e1;
        }

        loop_eids.push_back(via_eid);
        prev = cur;
        cur = next;
    } while (cur != v_start);

    return true;
}

std::vector<int> BoundaryCondition::arc_between_on_loop(const std::vector<int>& loop_vs,
                                                       const std::vector<int>& loop_eids,
                                                       int v0,
                                                       int v1,
                                                       bool forward) {
    const int n = (int)loop_vs.size();
    if (n == 0) return {};

    auto pos = [&](int v) {
        for (int i = 0; i < n; ++i) {
            if (loop_vs[i] == v) return i;
        }
        return -1;
    };

    const int i0 = pos(v0);
    const int i1 = pos(v1);
    if (i0 < 0 || i1 < 0) return {};

    std::vector<int> arc;
    if (forward) {
        for (int i = i0; i != i1; i = (i + 1) % n) {
            arc.push_back(loop_eids[i]);
        }
    } else {
        for (int i = i0; i != i1; i = (i - 1 + n) % n) {
            const int prev = (i - 1 + n) % n;
            arc.push_back(loop_eids[prev]);
        }
    }
    return arc;
}

double BoundaryCondition::polygon_area_sign(const DelaunayTriangulationResult& R,
                                            const std::vector<int>& loop_vs) {
    double A = 0.0;
    const int n = (int)loop_vs.size();
    for (int i = 0; i < n; ++i) {
        const auto& p = R.points[loop_vs[i]];
        const auto& q = R.points[loop_vs[(i + 1) % n]];
        A += p.x() * q.y() - p.y() * q.x();
    }
    return A;
}

std::unordered_map<int, std::vector<std::pair<int, int>>> BoundaryCondition::build_boundary_graph(
    const DelaunayTriangulationResult& R) {
    std::unordered_map<int, std::vector<std::pair<int, int>>> G;
    for (int eid = 0; eid < (int)R.edges.size(); ++eid) {
        const auto& e = R.edges[eid];
        if (!e.on_boundary) continue;
        if ((std::size_t)e.a >= R.points.size() || (std::size_t)e.b >= R.points.size()) continue;

        G[e.a].push_back({e.b, eid});
        G[e.b].push_back({e.a, eid});
    }
    return G;
}

std::vector<int> BoundaryCondition::find_boundary_path_edge_ids(const DelaunayTriangulationResult& R,
                                                                int v0,
                                                                int v1) {
    auto G = build_boundary_graph(R);
    if (!G.count(v0) || !G.count(v1)) return {};

    std::unordered_map<int, std::pair<int, int>> parent; // v -> (prev_v, via_eid)
    std::queue<int> q;
    q.push(v0);
    parent[v0] = {-1, -1};

    while (!q.empty()) {
        const int v = q.front();
        q.pop();
        if (v == v1) break;

        for (auto [nbr, eid] : G[v]) {
            if (!parent.count(nbr)) {
                parent[nbr] = {v, eid};
                q.push(nbr);
            }
        }
    }

    if (!parent.count(v1)) return {};

    std::vector<int> eids;
    for (int cur = v1; cur != v0;) {
        auto it = parent.find(cur);
        if (it == parent.end()) break;
        const int prev = it->second.first;
        const int eid = it->second.second;
        if (eid >= 0) eids.push_back(eid);
        cur = prev;
    }
    std::reverse(eids.begin(), eids.end());
    return eids;
}

void BoundaryCondition::begin_selection() {
    if (BoundaryCondition* prev_selected_bc = mesh_component()->edited_boundary_condition()) {
        prev_selected_bc->cancel_selection();
    }

    clear_selection();

    is_selected_ = true;
    mesh_component()->set_edited_boundary_condition(this);
}

void BoundaryCondition::apply_selection() {
    is_selected_ = false;
    mesh_component()->set_edited_boundary_condition(nullptr);
}

void BoundaryCondition::clear_selection() {
    start_point_ = s_invalid;
    end_point_ = s_invalid;
    edge_ids_.clear();
    has_geometry_ = false;
    arc_positions_.clear();
    loop_index_ = s_invalid;
    has_param_ = false;
}

void BoundaryCondition::cancel_selection() {
    clear_selection();
    is_selected_ = false;
    mesh_component()->set_edited_boundary_condition(nullptr);
}

void BoundaryCondition::rebuild() {
    edge_ids_ = compute_boundary_arc_edges(triangulation_result(), start_point_, end_point_, path_mode_);
}

void BoundaryCondition::capture_geometry_from_edges() {
    capture_geometry_from_edges(triangulation_result());
}

void BoundaryCondition::capture_geometry_from_edges(const DelaunayTriangulationResult& R) {
    if (edge_ids_.empty() || has_geometry_) return;
    if (R.edges.empty() || R.points.empty()) return;

    std::vector<int> arc_vertices;
    arc_vertices.reserve(edge_ids_.size() + 1);

    for (std::size_t i = 0; i < edge_ids_.size(); ++i) {
        const int eid = edge_ids_[i];
        if ((unsigned)eid >= (unsigned)R.edges.size()) continue;
        const auto& edge = R.edges[eid];

        if (i == 0) {
            arc_vertices.push_back(edge.a);
            arc_vertices.push_back(edge.b);
            continue;
        }

        if (arc_vertices.empty()) {
            arc_vertices.push_back(edge.a);
            arc_vertices.push_back(edge.b);
            continue;
        }

        const int last_v = arc_vertices.back();
        if (edge.a == last_v) {
            arc_vertices.push_back(edge.b);
        } else if (edge.b == last_v) {
            arc_vertices.push_back(edge.a);
        } else if (arc_vertices.size() > 1 && edge.a == arc_vertices[arc_vertices.size() - 2]) {
            std::reverse(arc_vertices.begin(), arc_vertices.end());
            arc_vertices.push_back(edge.b);
        } else if (arc_vertices.size() > 1 && edge.b == arc_vertices[arc_vertices.size() - 2]) {
            std::reverse(arc_vertices.begin(), arc_vertices.end());
            arc_vertices.push_back(edge.a);
        }
    }

    arc_positions_.clear();
    arc_positions_.reserve(arc_vertices.size());
    for (int v : arc_vertices) {
        if ((unsigned)v < (unsigned)R.points.size()) {
            arc_positions_.push_back(R.points[v].p);
        }
    }

    if (!arc_positions_.empty()) {
        start_point_ = arc_vertices.front();
        end_point_ = arc_vertices.back();
        has_geometry_ = true;
    }
}

void BoundaryCondition::remap_after_retriangulation() {
    const int old_edge_count = (int)edge_ids_.size();

    const auto& R = triangulation_result();
    PlanarMeshComponent* mc = mesh_component();
    if (!mc) {
        edge_ids_.clear();
        start_point_ = s_invalid;
        end_point_ = s_invalid;
        return;
    }

    if (!has_param_) {
        glm::dvec2 p0(0.0), p1(0.0);
        bool have_endpoints = false;

        if (has_geometry_ && !arc_positions_.empty()) {
            p0 = arc_positions_.front();
            p1 = arc_positions_.back();
            have_endpoints = true;
        } else if ((unsigned)start_point_ < (unsigned)R.points.size() &&
                   (unsigned)end_point_ < (unsigned)R.points.size()) {
            p0 = R.points[start_point_].p;
            p1 = R.points[end_point_].p;
            have_endpoints = true;
        } else if (!edge_ids_.empty()) {
            int v0 = s_invalid;
            int v1 = s_invalid;
            if (compute_chain_endpoints(R, edge_ids_, v0, v1) &&
                (unsigned)v0 < (unsigned)R.points.size() &&
                (unsigned)v1 < (unsigned)R.points.size()) {
                start_point_ = v0;
                end_point_ = v1;
                p0 = R.points[v0].p;
                p1 = R.points[v1].p;
                have_endpoints = true;
            }
        }

        if (have_endpoints) {
            const auto& loops = mc->boundary_loops();
            int best_L = -1;
            double best_score = std::numeric_limits<double>::infinity();
            ProjRes best_p0{};
            ProjRes best_p1{};

            for (int L = 0; L < (int)loops.size(); ++L) {
                double total_len = 0.0;
                double diag = 0.0;
                if (!compute_loop_diagnostics(loops[L].points, total_len, diag)) continue;

                double tmp_len0 = 0.0;
                double tmp_len1 = 0.0;
                const ProjRes r0 = project_point_to_loop_s(loops[L].points, p0, tmp_len0);
                const ProjRes r1 = project_point_to_loop_s(loops[L].points, p1, tmp_len1);
                const double score = std::max(r0.dist2, r1.dist2);

                if (score < best_score) {
                    best_score = score;
                    best_L = L;
                    best_p0 = r0;
                    best_p1 = r1;
                }
            }

            if (best_L >= 0) {
                loop_index_ = best_L;
                start_s_ = best_p0.s;
                end_s_ = best_p1.s;
                has_param_ = true;
            }
        }
    }

    if (!has_param_ || loop_index_ < 0) {
        LOGT_WARN(LogMath,
                  "BC remap failed: no stable boundary parameterization (had %d edges)",
                  old_edge_count);
        edge_ids_.clear();
        start_point_ = s_invalid;
        end_point_ = s_invalid;
        return;
    }

    const auto& loops = mc->boundary_loops();
    if (loop_index_ >= (int)loops.size()) {
        LOGT_WARN(LogMath, "BC remap failed: loop_index out of range (%d)", loop_index_);
        edge_ids_.clear();
        start_point_ = s_invalid;
        end_point_ = s_invalid;
        return;
    }

    double loop_total_len = 0.0;
    double loop_diag = 0.0;
    if (!compute_loop_diagnostics(loops[loop_index_].points, loop_total_len, loop_diag)) {
        LOGT_WARN(LogMath, "BC remap failed: loop diagnostics invalid (loop=%d)", loop_index_);
        edge_ids_.clear();
        start_point_ = s_invalid;
        end_point_ = s_invalid;
        return;
    }

    int new_start = -1;
    int new_end = -1;
    if (!pick_boundary_vertex_by_param(R,
                                       loops[loop_index_].points,
                                       loop_total_len,
                                       loop_diag,
                                       start_s_,
                                       new_start) ||
        !pick_boundary_vertex_by_param(R,
                                       loops[loop_index_].points,
                                       loop_total_len,
                                       loop_diag,
                                       end_s_,
                                       new_end)) {
        LOGT_WARN(LogMath, "BC remap failed: could not pick boundary vertices by parameter");
        edge_ids_.clear();
        start_point_ = s_invalid;
        end_point_ = s_invalid;
        return;
    }

    start_point_ = new_start;
    end_point_ = new_end;
    edge_ids_ = compute_boundary_arc_edges(R, start_point_, end_point_, path_mode_);

    has_geometry_ = false;
    arc_positions_.clear();
    capture_geometry_from_edges(R);

    LOGT_INFO(LogMath,
              "BC remap(param): loop=%d s=(%.6f->%.6f) v%d->v%d edges %d->%d",
              loop_index_,
              start_s_,
              end_s_,
              new_start,
              new_end,
              old_edge_count,
              (int)edge_ids_.size());
}

PlanarMeshComponent* BoundaryCondition::mesh_component() const {
    Object* owner = get_owner();
    return owner ? static_cast<PlanarMeshComponent*>(owner) : nullptr;
}

const DelaunayTriangulationResult& BoundaryCondition::triangulation_result() const {
    static const DelaunayTriangulationResult s_empty;
    const PlanarMeshComponent* mc = mesh_component();
    return mc ? mc->triangulation_result() : s_empty;
}

} // namespace fem