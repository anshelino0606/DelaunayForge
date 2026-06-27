#include "boundary_condition.h"

#include "geom/delaunay/delaunay_types.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "log_categories.h"
#include "math/fem/bc_value.h"
#include "math/math_.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
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
    out.dist2 = math::DINF;
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

static bool same_edge_set(const std::vector<int>& lhs, const std::vector<int>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    std::multiset<int> left(lhs.begin(), lhs.end());
    std::multiset<int> right(rhs.begin(), rhs.end());
    return left == right;
}

static bool compute_loop_diagnostics(const std::vector<Point2D>& loop,
                                     double& out_total_len,
                                     double& out_diag);

static std::vector<std::vector<Point2D>> extract_boundary_point_loops(
    const DelaunayTriangulationResult& R) {
    std::vector<std::vector<Point2D>> loops;
    std::unordered_set<int> visited_edges;
    visited_edges.reserve(R.edges.size());

    for (int eid = 0; eid < (int)R.edges.size(); ++eid) {
        if (visited_edges.count(eid) > 0) continue;

        const auto& edge = R.edges[eid];
        if (!edge.on_boundary) continue;

        std::vector<int> loop_vs;
        std::vector<int> loop_eids;
        if (!BoundaryCondition::extract_boundary_loop(R, edge.a, loop_vs, loop_eids)) continue;

        bool has_new_edge = false;
        for (int loop_eid : loop_eids) {
            if (visited_edges.count(loop_eid) == 0) {
                has_new_edge = true;
                break;
            }
        }
        if (!has_new_edge) continue;

        std::vector<Point2D> loop_points;
        loop_points.reserve(loop_vs.size());
        for (int vid : loop_vs) {
            if ((unsigned)vid >= (unsigned)R.points.size()) {
                loop_points.clear();
                break;
            }
            loop_points.push_back(R.points[vid]);
        }

        if (loop_points.size() >= 2) {
            loops.push_back(std::move(loop_points));
            for (int loop_eid : loop_eids) {
                visited_edges.insert(loop_eid);
            }
        }
    }

    return loops;
}

static bool select_best_loop_by_samples(const std::vector<std::vector<Point2D>>& loops,
                                        const std::vector<glm::dvec2>& sample_positions,
                                        int& out_loop_index,
                                        ProjRes& out_first_proj,
                                        ProjRes& out_last_proj,
                                        std::vector<double>& out_projected_samples) {
    out_loop_index = -1;
    out_projected_samples.clear();
    if (loops.empty() || sample_positions.empty()) return false;

    double best_score = math::DINF;

    for (int loop_index = 0; loop_index < (int)loops.size(); ++loop_index) {
        const auto& loop = loops[loop_index];
        double total_len = 0.0;
        double diag = 0.0;
        if (!compute_loop_diagnostics(loop, total_len, diag)) continue;

        double max_dist2 = 0.0;
        double sum_dist2 = 0.0;
        ProjRes first_proj{};
        ProjRes last_proj{};
        std::vector<double> projected_samples;
        projected_samples.reserve(sample_positions.size());

        for (std::size_t i = 0; i < sample_positions.size(); ++i) {
            double tmp_len = 0.0;
            const ProjRes pr = project_point_to_loop_s(loop, sample_positions[i], tmp_len);
            max_dist2 = std::max(max_dist2, pr.dist2);
            sum_dist2 += pr.dist2;
            projected_samples.push_back(pr.s);

            if (i == 0) first_proj = pr;
            if (i + 1 == sample_positions.size()) last_proj = pr;
        }

        const double avg_dist2 = sum_dist2 / std::max<std::size_t>(1, sample_positions.size());
        const double score = max_dist2 * 4.0 + avg_dist2;
        if (score < best_score) {
            best_score = score;
            out_loop_index = loop_index;
            out_first_proj = first_proj;
            out_last_proj = last_proj;
            out_projected_samples = std::move(projected_samples);
        }
    }

    return out_loop_index >= 0;
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
    double best_d2 = math::DINF;
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
    double best_on_loop_ds = math::DINF;
    double best_on_loop_d2 = math::DINF;

    int best_any = -1;
    double best_any_d2 = math::DINF;

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
    remap_segments_.clear();
    has_param_ = false;
    has_geometry_ = false;
    arc_positions_.clear();
}

void BoundaryCondition::set_edge_ids(const std::vector<int>& edge_ids) {
    edge_ids_ = edge_ids;

    has_geometry_ = false;
    arc_positions_.clear();
    remap_segments_.clear();
    has_param_ = false;

    capture_geometry_from_edges();
    capture_parameterization_from_edges();
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
        if (!e.valid_vertices(R.points.size()) || !e.on_boundary) continue;

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
    remap_segments_.clear();
    has_param_ = false;
}

void BoundaryCondition::cancel_selection() {
    clear_selection();
    is_selected_ = false;
    mesh_component()->set_edited_boundary_condition(nullptr);
}

void BoundaryCondition::rebuild() {
    edge_ids_ = compute_boundary_arc_edges(triangulation_result(), start_point_, end_point_, path_mode_);

    has_geometry_ = false;
    arc_positions_.clear();
    capture_geometry_from_edges();
    capture_parameterization_from_edges();
}

void BoundaryCondition::capture_geometry_from_edges() {
    capture_geometry_from_edges(triangulation_result());
}

void BoundaryCondition::capture_geometry_from_edges(const DelaunayTriangulationResult& R) {
    if (edge_ids_.empty() || has_geometry_) return;
    if (R.edges.empty() || R.points.empty()) return;

    std::vector<int> ordered_edge_ids;
    std::vector<int> arc_vertices;
    bool is_closed = false;
    if (!order_boundary_chain(R, edge_ids_, ordered_edge_ids, arc_vertices, is_closed) || is_closed) {
        return;
    }

    edge_ids_ = std::move(ordered_edge_ids);

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

// Find the nearest vertex to a world position.
// For remapping captured BC endpoints, the original boundary corner/endpoint
// coordinates are preserved in the refined mesh, so an exact-or-nearest scan
// over all vertices is sufficient and avoids relying on remapped EdgeInfo ids.
static int find_nearest_boundary_vertex(const DelaunayTriangulationResult& R,
                                        const glm::dvec2& target) {
    if (R.points.empty()) return -1;

    constexpr double exact_eps = 1e-9;
    constexpr double exact_eps2 = exact_eps * exact_eps;

    int nearest_vertex = -1;
    double nearest_d2 = math::DINF;

    for (std::size_t i = 0; i < R.points.size(); ++i) {
        const glm::dvec2& point = R.points[i].p;
        const double dx = point.x - target.x;
        const double dy = point.y - target.y;
        const double d2 = dx * dx + dy * dy;

        if (d2 <= exact_eps2) {
            return static_cast<int>(i);
        }
        if (d2 < nearest_d2) {
            nearest_d2 = d2;
            nearest_vertex = static_cast<int>(i);
        }
    }

    return nearest_vertex;
}

bool BoundaryCondition::capture_parameterization_from_edges() {
    return capture_parameterization_from_edges(triangulation_result());
}

bool BoundaryCondition::capture_parameterization_from_edges(const DelaunayTriangulationResult& R) {
    remap_segments_.clear();
    has_param_ = false;

    if (edge_ids_.empty() || R.edges.empty() || R.points.empty()) {
        return false;
    }

    if (!has_geometry_ || arc_positions_.empty()) {
        capture_geometry_from_edges(R);
    }

    // Build the BC-local subgraph from edge_ids_ (boundary edges only).
    std::unordered_map<int, std::vector<std::pair<int, int>>> graph;
    for (int eid : edge_ids_) {
        if ((unsigned)eid >= (unsigned)R.edges.size()) continue;
        const auto& e = R.edges[eid];
        if (!e.on_boundary) continue;
        graph[e.a].push_back({e.b, eid});
        graph[e.b].push_back({e.a, eid});
    }
    if (graph.empty()) return false;

    // Collect chain start vertices (degree-1 in local subgraph).
    std::vector<int> chain_starts;
    for (const auto& [v, nbrs] : graph) {
        if ((int)nbrs.size() == 1) chain_starts.push_back(v);
    }
    // Closed loop has no degree-1 vertices: pick any vertex.
    if (chain_starts.empty()) {
        chain_starts.push_back(graph.begin()->first);
    }

    // Walk each chain and record its world-space start/end positions.
    std::unordered_set<int> used_edges;
    for (int sv : chain_starts) {
        // Skip if all edges from sv are already walked (chain visited from other end).
        bool any_unvisited = false;
        for (const auto& [nb, eid] : graph[sv]) {
            if (!used_edges.count(eid)) { any_unvisited = true; break; }
        }
        if (!any_unvisited) continue;

        int cur = sv;
        int end_v = sv;
        while (true) {
            bool moved = false;
            for (const auto& [nb, eid] : graph[cur]) {
                if (used_edges.count(eid)) continue;
                used_edges.insert(eid);
                cur = nb;
                moved = true;
                break;
            }
            if (!moved) { end_v = cur; break; }
        }

        if ((unsigned)sv  < (unsigned)R.points.size() &&
            (unsigned)end_v < (unsigned)R.points.size()) {
            // Detect which path_mode_ actually matches the selected edges.
            BoundaryConditionPathMode detected_mode = path_mode_;
            const BoundaryConditionPathMode modes[] = {
                BoundaryConditionPathMode::Shorter,
                BoundaryConditionPathMode::Longer,
                BoundaryConditionPathMode::CW,
                BoundaryConditionPathMode::CCW,
            };
            for (BoundaryConditionPathMode m : modes) {
                const auto arc = compute_boundary_arc_edges(R, sv, end_v, m);
                if (same_edge_set(arc, edge_ids_)) {
                    detected_mode = m;
                    break;
                }
            }

            remap_segments_.push_back({R.points[sv].p, R.points[end_v].p, detected_mode});
        }
    }

    // Also update start/end vertex IDs for the first chain.
    if (!remap_segments_.empty() && !chain_starts.empty()) {
        start_point_ = chain_starts.front();
        // Re-walk first chain to find end_v (we already stored end_world, recover v).
        // Just search R.points for the closest point to end_world.
        const glm::dvec2& end_world = remap_segments_.front().end_world;
        int best = -1;
        double best_d2 = math::DINF;
        for (int i = 0; i < (int)R.points.size(); ++i) {
            const glm::dvec2& p = R.points[i].p;
            const double d2 = (p.x - end_world.x)*(p.x - end_world.x) +
                              (p.y - end_world.y)*(p.y - end_world.y);
            if (d2 < best_d2) { best_d2 = d2; best = i; }
        }
        end_point_ = best;
    }

    has_param_ = !remap_segments_.empty();
    return has_param_;
}

void BoundaryCondition::remap_after_retriangulation() {
    const int old_edge_count = (int)edge_ids_.size();

    if (remap_segments_.empty()) {
        LOGT_WARN(LogMath,
                  "BC remap failed: no stable boundary parameterization (had %d edges)",
                  old_edge_count);
        edge_ids_.clear();
        start_point_ = s_invalid;
        end_point_ = s_invalid;
        return;
    }

    const auto& R = triangulation_result();

    std::vector<int> new_edge_ids;
    int new_start = s_invalid;
    int new_end   = s_invalid;

    for (const auto& seg : remap_segments_) {
        const int v0 = find_nearest_boundary_vertex(R, seg.start_world);
        const int v1 = find_nearest_boundary_vertex(R, seg.end_world);
        if (v0 < 0 || v1 < 0 || v0 == v1) continue;

        const auto arc = compute_boundary_arc_edges(R, v0, v1, seg.mode);
        if (arc.empty()) continue;

        new_edge_ids.insert(new_edge_ids.end(), arc.begin(), arc.end());
        if (new_start == s_invalid) new_start = v0;
        new_end = v1;
    }

    if (new_edge_ids.empty()) {
        LOGT_WARN(LogMath,
                  "BC remap failed: arc reconstruction produced no edges (had %d edges)",
                  old_edge_count);
        edge_ids_.clear();
        start_point_ = s_invalid;
        end_point_ = s_invalid;
        return;
    }

    edge_ids_    = std::move(new_edge_ids);
    start_point_ = new_start;
    end_point_   = new_end;

    has_geometry_ = false;
    arc_positions_.clear();
    capture_geometry_from_edges(R);

    LOGT_INFO(LogMath,
              "BC remap: %d seg(s) %d->%d edges",
              (int)remap_segments_.size(),
              old_edge_count,
              (int)edge_ids_.size());
}

bool BoundaryCondition::order_boundary_chain(const DelaunayTriangulationResult& R,
                                             const std::vector<int>& edge_ids,
                                             std::vector<int>& ordered_edge_ids,
                                             std::vector<int>& ordered_vertices,
                                             bool& is_closed) {
    ordered_edge_ids.clear();
    ordered_vertices.clear();
    is_closed = false;

    if (edge_ids.empty()) return false;

    std::unordered_map<int, std::vector<std::pair<int, int>>> graph;
    graph.reserve(edge_ids.size() * 2);

    int valid_edges = 0;
    for (int eid : edge_ids) {
        if ((unsigned)eid >= (unsigned)R.edges.size()) continue;
        const auto& edge = R.edges[eid];
        if (!edge.on_boundary) continue;

        graph[edge.a].push_back({edge.b, eid});
        graph[edge.b].push_back({edge.a, eid});
        ++valid_edges;
    }

    if (valid_edges == 0 || graph.empty()) return false;

    std::vector<int> endpoints;
    endpoints.reserve(2);
    for (const auto& [vertex, neighbors] : graph) {
        if (neighbors.size() == 1) {
            endpoints.push_back(vertex);
        } else if (neighbors.size() != 2) {
            return false;
        }
    }

    if (endpoints.size() == 2) {
        is_closed = false;
    } else if (endpoints.empty()) {
        is_closed = true;
    } else {
        return false;
    }

    const int start_vertex = is_closed ? graph.begin()->first : endpoints.front();
    int current = start_vertex;
    int previous = s_invalid;
    std::unordered_set<int> used_edges;
    used_edges.reserve(edge_ids.size());
    ordered_vertices.push_back(current);

    while (true) {
        int next_vertex = s_invalid;
        int next_edge = s_invalid;

        const auto graph_it = graph.find(current);
        if (graph_it == graph.end()) return false;

        for (const auto& [neighbor, eid] : graph_it->second) {
            if (used_edges.count(eid) > 0) continue;
            if (!is_closed && neighbor == previous && graph_it->second.size() > 1) continue;

            next_vertex = neighbor;
            next_edge = eid;
            break;
        }

        if (next_edge == s_invalid) {
            break;
        }

        used_edges.insert(next_edge);
        ordered_edge_ids.push_back(next_edge);
        ordered_vertices.push_back(next_vertex);
        previous = current;
        current = next_vertex;

        if (is_closed && current == start_vertex) {
            break;
        }
    }

    if ((int)used_edges.size() != valid_edges) return false;
    if (!is_closed && ordered_vertices.size() != ordered_edge_ids.size() + 1) return false;

    return true;
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