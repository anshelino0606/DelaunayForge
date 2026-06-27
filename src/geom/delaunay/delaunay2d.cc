#include "delaunay2d.h"
#include "log_categories.h"
#include "geom/geom2d/tri.h"
#include "geom/geom2d/predicate.h"
#include "math/math_.h"
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <cmath>
#include <iostream>
#include <numeric>
#include <chrono>
#include <span>


namespace fem {

namespace {

/// Compute signed area (straight or gay)
[[nodiscard]] inline double orient_val(
    const glm::dvec2& a, 
    const glm::dvec2& b, 
    const glm::dvec2& c) noexcept 
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

/// Check if two edges share an endpoint
[[nodiscard]] inline bool share_endpoint(int a, int b, int c, int d) noexcept {
    return (a == c) || (a == d) || (b == c) || (b == d);
}

/// Test if two line segments properly intersect (not at endpoints)
[[nodiscard]] bool segments_intersect(
    const glm::dvec2& A, const glm::dvec2& B,
    const glm::dvec2& C, const glm::dvec2& D,
    double eps) noexcept
{
    const double o1 = orient_val(A, B, C);
    const double o2 = orient_val(A, B, D);
    const double o3 = orient_val(C, D, A);
    const double o4 = orient_val(C, D, B);

    auto sign = [eps](double v) -> int {
        if (v > eps) return 1;
        if (v < -eps) return -1;
        return 0;
    };

    const int s1 = sign(o1), s2 = sign(o2);
    const int s3 = sign(o3), s4 = sign(o4);
    
    return (s1 * s2 < 0) && (s3 * s4 < 0);
}


[[nodiscard]] bool pointInPoly(std::span<const Point2D> P,
                               std::span<const int> idx,
                               double x, double y) noexcept
{
    if (idx.size() < 3) return true;

    bool inside = false;
    std::size_t j = idx.size() - 1;

    for (std::size_t i = 0; i < idx.size(); j = i++) {
        const auto& a = P[static_cast<std::size_t>(idx[i])];
        const auto& b = P[static_cast<std::size_t>(idx[j])];

        const bool cond = (a.y() > y) != (b.y() > y);
        if (!cond) continue;
        const double denom = (b.y() - a.y()) + 1e-300;
        const double x_on_edge = (b.x() - a.x()) * (y - a.y()) / denom + a.x();

        if (x < x_on_edge) inside = !inside;
    }
    return inside;
}

static inline int opposite_vertex(const Tri& t, int a, int b) noexcept {
    for (int i = 0; i < 3; ++i) {
        const int v = t.v[i];
        if (v != a && v != b) return v;
    }
    return -1;
}

static void seed_domain_points(std::vector<Point2D>& pts,
                               const std::vector<std::vector<int>>& loop_idx,
                               const std::vector<Point2D>& allPts,
                               double h)
{
    if (loop_idx.empty() || loop_idx[0].size() < 3) return;

    auto inside = [&](double x,double y){
        if (!pointInPoly(allPts, loop_idx[0], x, y)) return false;
        for (size_t h=1; h<loop_idx.size(); ++h)
            if (pointInPoly(allPts, loop_idx[h], x, y)) return false;
        return true;
    };

    // bbox of outer
    double xmin=1e300,xmax=-1e300,ymin=1e300,ymax=-1e300;
    for (int id : loop_idx[0]) {
        xmin = std::min(xmin, allPts[id].x());
        xmax = std::max(xmax, allPts[id].x());
        ymin = std::min(ymin, allPts[id].y());
        ymax = std::max(ymax, allPts[id].y());
    }

    const double step = std::max(1.0, 0.9*h);
    for (double y=ymin+step; y<=ymax-step; y+=step)
        for (double x=xmin+step; x<=xmax-step; x+=step)
            if (inside(x,y)) pts.emplace_back(x, y, (int)pts.size());
}

static bool points_on_circle(const std::vector<Point2D>& pts, double eps = 1e-10) {
    if (pts.size() < 4) return false;

    glm::dvec2 A = pts[0].p, B = pts[1].p, C = pts[2].p;
    double d = 2.0 * (A.x*(B.y - C.y) + B.x*(C.y - A.y) + C.x*(A.y - B.y));
    if (std::abs(d) < eps) return false;
    double ux = ((A.x*A.x + A.y*A.y)*(B.y - C.y) +
                 (B.x*B.x + B.y*B.y)*(C.y - A.y) +
                 (C.x*C.x + C.y*C.y)*(A.y - B.y)) / d;
    double uy = ((A.x*A.x + A.y*A.y)*(C.x - B.x) +
                 (B.x*B.x + B.y*B.y)*(A.x - C.x) +
                 (C.x*C.x + C.y*C.y)*(B.x - A.x)) / d;
    glm::dvec2 O(ux, uy);
    double R = std::hypot(A.x - ux, A.y - uy);

    for (const auto& p : pts) {
        double r = std::hypot(p.x() - ux, p.y() - uy);
        if (std::abs(r - R) > 1e-6 * R) return false;
    }
    return true;
}

static void build_edges_for_result(DelaunayTriangulationResult& R) {
    using detail::EdgeKey;
    using detail::U64Hash;

    struct AccEdge {
        int a = -1, b = -1;
        int tri_left  = -1;
        int tri_right = -1;
    };

    const std::size_t tri_n = R.triangles.size();
    R.tri2edge.assign(tri_n, {-1, -1, -1});

    std::unordered_map<EdgeKey, int, U64Hash> edge_index;
    edge_index.reserve(tri_n * 3);

    std::vector<AccEdge> acc;
    acc.reserve(tri_n * 3);

    auto add_edge = [&](int a, int b, int t) -> int {
        const EdgeKey k = detail::pack_edge(a, b);
        auto [it, inserted] = edge_index.emplace(k, static_cast<int>(acc.size()));
        if (inserted) {
            AccEdge e;
            e.a = (a < b) ? a : b;
            e.b = (a < b) ? b : a;
            e.tri_left = t;
            acc.push_back(e);
            return it->second;
        }
        auto& e = acc[static_cast<std::size_t>(it->second)];
        if (e.tri_left == -1) e.tri_left = t;
        else                 e.tri_right = t;
        return it->second;
    };

    for (std::size_t ti = 0; ti < tri_n; ++ti) {
        const auto& T = R.triangles[ti];
        const int t = static_cast<int>(ti);

        const int e0 = add_edge(T.v[0], T.v[1], t);
        const int e1 = add_edge(T.v[1], T.v[2], t);
        const int e2 = add_edge(T.v[2], T.v[0], t);

        R.tri2edge[ti] = {e0, e1, e2};
    }

    R.edges.clear();
    R.edges.reserve(acc.size());

    for (const auto& e : acc) {
        EdgeInfo out{};
        out.a = e.a;
        out.b = e.b;
        out.tri_left  = e.tri_left;
        out.tri_right = e.tri_right;
        out.on_boundary = (e.tri_right == -1);
        R.edges.push_back(out);
    }

    // for (auto& p : R.points) p.on_boundary = false;
    // for (const auto& e : R.edges) {
    //     if (!e.on_boundary) continue;
    //     if ((std::size_t)e.a < R.points.size()) R.points[e.a].on_boundary = true;
    //     if ((std::size_t)e.b < R.points.size()) R.points[e.b].on_boundary = true;
    // }
}

static inline bool point_on_segment_eps(const glm::dvec2& P,
                                        const glm::dvec2& A,
                                        const glm::dvec2& B,
                                        double eps)
{
    const double cross = orient_val(A, B, P);
    if (std::abs(cross) > eps) return false;

    const glm::dvec2 AP = P - A;
    const glm::dvec2 AB = B - A;
    const double dot = AP.x*AB.x + AP.y*AB.y;
    if (dot < -eps) return false;

    const double ab2 = AB.x*AB.x + AB.y*AB.y;
    if (dot > ab2 + eps) return false;

    return true;
}

static void remove_dangling_vertices(DelaunayTriangulationResult& R) {
    if (R.points.empty() || R.triangles.empty()) return;

    const std::size_t n_points = R.points.size();

    std::vector<std::uint8_t> used(n_points, 0);
    for (const auto& tri : R.triangles) {
        if (!tri.valid) continue;
        for (int k = 0; k < 3; ++k) {
            const int v = tri.v[k];
            if (detail::valid_index(v, n_points)) used[static_cast<std::size_t>(v)] = 1;
        }
    }

    std::size_t used_count = 0;
    for (auto u : used) used_count += (u != 0);

    if (used_count == n_points) return;

    std::vector<int> old_to_new(n_points, -1);
    std::vector<Point2D> new_points;
    new_points.reserve(used_count);

    int new_id = 0;
    for (std::size_t i = 0; i < n_points; ++i) {
        if (!used[i]) continue;
        old_to_new[i] = new_id;
        auto p = R.points[i];
        p.id = new_id;
        new_points.push_back(p);
        ++new_id;
    }

    auto remap = [&](int& v) noexcept {
        if (!detail::valid_index(v, n_points)) return;
        v = old_to_new[static_cast<std::size_t>(v)];
    };

    for (auto& tri : R.triangles) {
        if (!tri.valid) continue;
        remap(tri.v[0]); remap(tri.v[1]); remap(tri.v[2]);
    }
    for (auto& e : R.boundary_edges) { remap(e.a); remap(e.b); }
    for (auto& e : R.edges)          { remap(e.a); remap(e.b); }
    for (auto& v : R.tri2vert)       { remap(v[0]); remap(v[1]); remap(v[2]); }

    // rebuild vert2tri
    R.vert2tri.assign(new_points.size(), {});
    for (std::size_t ti = 0; ti < R.triangles.size(); ++ti) {
        const auto& t = R.triangles[ti];
        if (!t.valid) continue;
        const int tri_idx = static_cast<int>(ti);
        if (detail::valid_index(t.v[0], new_points.size())) R.vert2tri[t.v[0]].push_back(tri_idx);
        if (detail::valid_index(t.v[1], new_points.size())) R.vert2tri[t.v[1]].push_back(tri_idx);
        if (detail::valid_index(t.v[2], new_points.size())) R.vert2tri[t.v[2]].push_back(tri_idx);
    }

    R.points = std::move(new_points);
}

} // anonymous namespace

DelaunayTriangulator::DelaunayTriangulator(const DelaunayTriangulationConfig& cfg) : config(cfg) {}

DelaunayTriangulationResult DelaunayTriangulator::triangulate(const std::vector<Point2D>& input_points) {    
    if (points_on_circle(input_points)) {
        LOGT_INFO(LogGeometry, "Detected concyclic point set — using circle-optimized triangulation.");
        return triangulate_circle(input_points);
    }

    DelaunayTriangulationResult result;
    if (input_points.size() < 3) {
        result.points = input_points;
        return result;
    }

    points = input_points;
    triangles.clear();
    boundary_edges.clear();
    
    for (size_t i = 0; i < points.size(); ++i) {
        points[i].id = static_cast<int>(i);
    }

    add_super_triangle();
    
    bowyer_watson();
    
    remove_super_triangle();
    
    for (int i = 0; i < static_cast<int>(triangles.size()); ++i) {
        Tri& t = triangles[i];
        if (!t.valid) continue;

        for (int j = 0; j < 3; ++j) {
            if (!t.valid_vertex(j, points.size())) {
                t.valid = false;
                break;
            }
        }
    }
        
    build_adjacency();
    
    // Quality improvement
    if (config.enable_lloyd_smoothing ||
        config.enable_edge_flipping   ||
        config.enable_min_angle_refinement ||
        config.enable_sizing_refinement) {
        improve_mesh_quality();
        build_adjacency();
    }
    result.points = points;
    for (const auto& tri : triangles) {
        if (tri.valid) {
            result.triangles.push_back(tri);
        }
    }
    
    result.boundary_edges = boundary_edges;

    result.tri2vert.resize(result.triangles.size());
    result.tri_neighbors.resize(result.triangles.size());
    for (size_t i = 0; i < result.triangles.size(); ++i) {
        const Tri& t = result.triangles[i];
        result.tri2vert[i] = {t.v[0], t.v[1], t.v[2]};
        result.tri_neighbors[i] = {t.neighbors[0], t.neighbors[1], t.neighbors[2]};
    }
    
    result.vert2tri.resize(result.points.size());
    for (size_t i = 0; i < result.points.size(); ++i) {
        result.vert2tri[i] = points[i].incident_triangles;
    }
    result.edges = edges_cache_;
    result.tri2edge = tri2edge_cache_;
    
    compute_statistics(result);

    remove_dangling_vertices(result);
    
    return result;
}

void DelaunayTriangulator::add_super_triangle() {
    // Find bounding box
    double xmin = math::DMAX;
    double xmax = math::DMIN;
    double ymin = math::DMAX;
    double ymax = math::DMIN;
    
    for (const auto& p : points) {
        xmin = std::min(xmin, p.x());
        xmax = std::max(xmax, p.x());
        ymin = std::min(ymin, p.y());
        ymax = std::max(ymax, p.y());
    }
    
    double dx = xmax - xmin;
    double dy = ymax - ymin;
    double dmax = std::max(dx, dy) * 10.0;
    
    double cx = (xmin + xmax) * 0.5;
    double cy = (ymin + ymax) * 0.5;
    
    points.emplace_back(cx, cy + 2*dmax, static_cast<int>(points.size()));
    points.emplace_back(cx - 1.732*dmax, cy - dmax, static_cast<int>(points.size()));  
    points.emplace_back(cx + 1.732*dmax, cy - dmax, static_cast<int>(points.size()));
    
    int s0 = static_cast<int>(points.size()) - 3;
    int s1 = static_cast<int>(points.size()) - 2;
    int s2 = static_cast<int>(points.size()) - 1;
    
    (void)triangles.alloc(s0, s1, s2);
}

void DelaunayTriangulator::bowyer_watson() {
    int original_count = static_cast<int>(points.size()) - 3; // Exclude super triangle
    
    for (int i = 0; i < original_count; ++i) {
        const Point2D& new_point = points[i];
        
        // Find bad triangles
        std::vector<int> bad_triangles = find_bad_triangles(new_point);
        
        // Extract cavity boundary
        std::vector<Edge> cavity_boundary = extract_cavity_boundary(bad_triangles);
        
        // Remove bad triangles
        remove_triangles(bad_triangles);
        
        // Retriangulate cavity
        retriangulate_cavity(i, cavity_boundary);
    }
}

std::vector<int> DelaunayTriangulator::find_bad_triangles(const Point2D& point) {
    std::vector<int> bad;
    glm::dvec2 p = point.p;
    
    for (size_t i = 0; i < triangles.size(); ++i) {
        if (!triangles[i].valid) continue;
        
        const Tri& tri = triangles[i];
        glm::dvec2 a = points[tri.v[0]].p;
        glm::dvec2 b = points[tri.v[1]].p;
        glm::dvec2 c = points[tri.v[2]].p;
        
        // Ensure CCW orientation
        if (geom2d::pred::orient_sign(a, b, c) < 0) {
            std::swap(triangles[i].v[1], triangles[i].v[2]);
            b = points[triangles[i].v[1]].p;
            c = points[triangles[i].v[2]].p;
        }
        
        if (geom2d::pred::incircle_ccw(a, b, c, p, config.epsilon) > 0) {
            bad.push_back(static_cast<int>(i));
        }
    }
    
    return bad;
}

std::vector<Edge> DelaunayTriangulator::extract_cavity_boundary(const std::vector<int>& bad_triangles) {
    using detail::EdgeKey;
    using detail::U64Hash;

    std::unordered_map<EdgeKey, int, U64Hash> edge_count;
    edge_count.reserve(bad_triangles.size() * 3);

    for (int tri_idx : bad_triangles) {
        const Tri& tri = triangles[tri_idx];
        edge_count[detail::pack_edge(tri.v[0], tri.v[1])] += 1;
        edge_count[detail::pack_edge(tri.v[1], tri.v[2])] += 1;
        edge_count[detail::pack_edge(tri.v[2], tri.v[0])] += 1;
    }

    std::vector<Edge> boundary;
    boundary.reserve(edge_count.size());

    for (const auto& [k, cnt] : edge_count) {
        if (cnt != 1) continue;
        const int a = static_cast<int>(k >> 32);
        const int b = static_cast<int>(k & 0xffffffffu);
        boundary.emplace_back(a, b);
    }

    return boundary;
}


void DelaunayTriangulator::remove_triangles(const std::vector<int>& triangle_ids) {
    for (int id : triangle_ids) { triangles.erase(id); }
}

void DelaunayTriangulator::retriangulate_cavity(int point_idx, const std::vector<Edge>& boundary) {
    for (const Edge& edge : boundary) {
        const int tid = triangles.alloc(edge.a, edge.b, point_idx);
        Tri& new_tri = triangles[tid];

        glm::dvec2 a = points[new_tri.v[0]].p;
        glm::dvec2 b = points[new_tri.v[1]].p;
        glm::dvec2 c = points[new_tri.v[2]].p;

        if (geom2d::pred::orient_sign(a, b, c) < 0) {
            std::swap(new_tri.v[0], new_tri.v[1]);
        }
    }
}


void DelaunayTriangulator::remove_super_triangle() {
    const int original_count = static_cast<int>(points.size()) - 3;
    const int s0 = original_count;
    const int s1 = original_count + 1;
    const int s2 = original_count + 2;

    triangles.compact_keep_if([&](const Tri& tri) -> bool {
        for (int j = 0; j < 3; ++j) {
            const int v = tri.v[j];
            if (v == s0 || v == s1 || v == s2) return false;
            if (v < 0 || v >= original_count) {
                LOGT_WARN(LogGeometry,
                          "Triangle with out-of-range vertex %d (expected 0..%d).",
                          v, original_count - 1);
                return false;
            }
        }
        return true;
    });

    points.resize(static_cast<std::size_t>(original_count));
    for (int i = 0; i < original_count; ++i) {
        points[static_cast<std::size_t>(i)].id = i;
    }
}


void DelaunayTriangulator::build_adjacency() {
    for (auto& p : points) {
        p.incident_triangles.clear();
    }
    
    for (size_t i = 0; i < triangles.size(); ++i) {
        Tri& t = triangles[i];
        if (!t.valid) continue;
        
        bool valid = true;
        for (int j = 0; j < 3; ++j) {
            if (!t.valid_vertex(j, points.size())) {
                LOGT_ERROR(LogGeometry,
                    "Triangle %zu references invalid vertex %d (point count: %zu)",
                    i,
                    t.v[j],
                    points.size());
                valid = false;
            }
        }

        if (!valid) {
            t.valid = false;
            continue;
        }
        
        points[t.v[0]].incident_triangles.push_back(static_cast<int>(i));
        points[t.v[1]].incident_triangles.push_back(static_cast<int>(i));
        points[t.v[2]].incident_triangles.push_back(static_cast<int>(i));
    }
    
    update_triangle_neighbors();
    
    edge_index_cache_.clear();
    edges_cache_.clear();
    tri2edge_cache_.assign(triangles.size(), {-1,-1,-1});

    edge_index_cache_.reserve(triangles.size() * 3);
    edges_cache_.reserve(triangles.size() * 3);

    auto add_edge = [&](int a, int b) -> int {
        if (a > b) std::swap(a, b);
        
        // Validate edge vertices
        if (a < 0 || static_cast<size_t>(a) >= points.size() ||
            b < 0 || static_cast<size_t>(b) >= points.size()) {
            return -1;
        }
        
        detail::EdgeKey k = detail::pack_edge(a, b);

        auto it = edge_index_cache_.find(k);
        if (it != edge_index_cache_.end()) return it->second;
        
        int id = static_cast<int>(edges_cache_.size());
        edge_index_cache_[k] = id;
        EdgeInfo ei{};
        ei.a = a;
        ei.b = b;
        ei.tri_left  = -1;
        ei.tri_right = -1;
        ei.on_boundary = false;
        edges_cache_.push_back(ei);
        return id;
    };

    for (size_t i = 0; i < triangles.size(); ++i) {
        const Tri& t = triangles[i];
        if (!t.valid) continue;
        
        int e0 = add_edge(t.v[0], t.v[1]);
        int e1 = add_edge(t.v[1], t.v[2]);
        int e2 = add_edge(t.v[2], t.v[0]);
        
        if (e0 >= 0 && e1 >= 0 && e2 >= 0) {
            tri2edge_cache_[i] = {e0, e1, e2};
        }
    }

    // Set triangle adjacency for edges
    for (size_t i = 0; i < triangles.size(); ++i) {
        const Tri& t = triangles[i];
        if (!t.valid) continue;
        
        auto set_adj = [&](int eid) {
            if (eid < 0 || static_cast<size_t>(eid) >= edges_cache_.size()) return;
            auto& ei = edges_cache_[eid];
            if (ei.tri_left == -1) {
                ei.tri_left = static_cast<int>(i);
            } else if (ei.tri_right == -1) {
                ei.tri_right = static_cast<int>(i);
            }
        };
        
        auto e = tri2edge_cache_[i];
        set_adj(e[0]);
        set_adj(e[1]);
        set_adj(e[2]);
    }

    for (auto& ei : edges_cache_) {
        if (ei.tri_right == -1) {
            ei.on_boundary = true;
        }
    }
    // for (auto& p : points) p.on_boundary = false;
    // for (const auto& ei : edges_cache_) if (ei.on_boundary) {
    //     if ((size_t)ei.a < points.size()) points[ei.a].on_boundary = true;
    //     if ((size_t)ei.b < points.size()) points[ei.b].on_boundary = true;
    // }
}


void fem::DelaunayTriangulator::update_triangle_neighbors() {
    using detail::EdgeKey;
    using detail::U64Hash;

    struct Pair {
        int a = -1;
        int b = -1;
        void add(int t) noexcept { (a == -1) ? a = t : b = t; }
        [[nodiscard]] int other(int t) const noexcept { return (a == t) ? b : (b == t ? a : -1); }
    };

    std::unordered_map<EdgeKey, Pair, U64Hash> edge2pair;
    edge2pair.reserve(triangles.size() * 3);

    for (std::size_t i = 0; i < triangles.size(); ++i) {
        if (!triangles[i].valid) continue;
        const auto& tri = triangles[i];
        const int t = static_cast<int>(i);

        edge2pair[detail::pack_edge(tri.v[0], tri.v[1])].add(t);
        edge2pair[detail::pack_edge(tri.v[1], tri.v[2])].add(t);
        edge2pair[detail::pack_edge(tri.v[2], tri.v[0])].add(t);
    }

    for (std::size_t i = 0; i < triangles.size(); ++i) {
        if (!triangles[i].valid) continue;
        auto& tri = triangles[i];
        const int t = static_cast<int>(i);

        tri.neighbors[0] = edge2pair[detail::pack_edge(tri.v[0], tri.v[1])].other(t);
        tri.neighbors[1] = edge2pair[detail::pack_edge(tri.v[1], tri.v[2])].other(t);
        tri.neighbors[2] = edge2pair[detail::pack_edge(tri.v[2], tri.v[0])].other(t);
    }
}


void DelaunayTriangulator::improve_mesh_quality() {
    if (config.enable_lloyd_smoothing && active_boundary_loops_.empty() && active_boundary_loop_.empty()) {
        lloyd_smoothing();
    }
    
    if (config.enable_edge_flipping) {
        edge_flipping_pass();
    }

    if (config.enable_min_angle_refinement && config.min_angle_threshold > 0.0f) {
        refine_min_angle(config.min_angle_threshold, config.refine_max_steiner);
    }

    refine_to_density();
}

void DelaunayTriangulator::enforce_active_loop_constraints_if_any() {
    if (!active_boundary_loops_.empty()) {
        build_constraints_from_loops(active_boundary_loops_);
        enforce_constraints();
        return;
    }
    if (!active_boundary_loop_.empty()) {
        build_constraints_from_loops({active_boundary_loop_});
        enforce_constraints();
        return;
    }
}

void DelaunayTriangulator::retriangulate_in_place() {
    triangles.clear();
    boundary_edges.clear();
    for (size_t i = 0; i < points.size(); ++i) {
        points[i].id = static_cast<int>(i);
    }

    add_super_triangle();
    bowyer_watson();
    remove_super_triangle();

    // Preserve loop constraints across any retriangulation caused by smoothing/refinement.
    if (!active_boundary_loops_.empty() || !active_boundary_loop_.empty()) {
        build_adjacency();
        enforce_active_loop_constraints_if_any();
        build_adjacency();
    } else {
        build_adjacency();
    }
}

void DelaunayTriangulator::retriangulate_in_place_unconstrained_() {
    triangles.clear();
    boundary_edges.clear();
    for (size_t i = 0; i < points.size(); ++i) {
        points[i].id = static_cast<int>(i);
    }

    add_super_triangle();
    bowyer_watson();
    remove_super_triangle();

    build_adjacency();
}

void DelaunayTriangulator::rebuild_active_loop_bboxes_(std::span<const Point2D> pts) {
    active_loop_bboxes_.clear();
    if (active_boundary_loops_.empty()) return;

    active_loop_bboxes_.resize(active_boundary_loops_.size());
    for (std::size_t li = 0; li < active_boundary_loops_.size(); ++li) {
        LoopBBox bb;
        const auto& L = active_boundary_loops_[li];
        if (L.empty()) {
            active_loop_bboxes_[li] = bb;
            continue;
        }

        for (int vid : L) {
            if (vid < 0 || static_cast<std::size_t>(vid) >= pts.size()) continue;
            const auto& p = pts[static_cast<std::size_t>(vid)];
            const double x = p.x();
            const double y = p.y();
            bb.xmin = std::min(bb.xmin, x);
            bb.xmax = std::max(bb.xmax, x);
            bb.ymin = std::min(bb.ymin, y);
            bb.ymax = std::max(bb.ymax, y);
        }
        active_loop_bboxes_[li] = bb;
    }
}

bool DelaunayTriangulator::is_inside_active_domain(double x, double y) const {
    if (!active_boundary_loops_.empty()) {
        if (active_boundary_loops_[0].size() < 3) return true;
        if (!active_loop_bboxes_.empty() && active_loop_bboxes_[0].contains(x, y) == false) return false;
        if (!pointInPoly(points, active_boundary_loops_[0], x, y)) return false;
        for (size_t h = 1; h < active_boundary_loops_.size(); ++h) {
            if (active_boundary_loops_[h].size() < 3) continue;
            if (!active_loop_bboxes_.empty() && h < active_loop_bboxes_.size()) {
                if (!active_loop_bboxes_[h].contains(x, y)) continue;
            }
            if (pointInPoly(points, active_boundary_loops_[h], x, y)) return false;
        }
        return true;
    }

    if (!active_boundary_loop_.empty()) {
        if (active_boundary_loop_.size() < 3) return true;
        return pointInPoly(points, active_boundary_loop_, x, y);
    }

    return true;
}

void DelaunayTriangulator::lloyd_smoothing() {
    for (int iter = 0; iter < config.lloyd_iterations; ++iter) {
        std::vector<Point2D> new_positions = points;
        
        // For each interior point, move it to the centroid of its Voronoi cell
        for (size_t i = 0; i < points.size(); ++i) {
            if (points[i].on_boundary) continue;
            
            const auto& incident_tris = points[i].incident_triangles;
            if (incident_tris.empty()) continue;
            
            std::vector<glm::dvec2> voronoi_vertices;
            
            for (int tri_id : incident_tris) {
                if (tri_id < 0 || tri_id >= triangles.size() || !triangles[tri_id].valid) continue;
                
                const Tri& tri = triangles[tri_id];
                glm::dvec2 A = points[tri.v[0]].p;
                glm::dvec2 B = points[tri.v[1]].p;
                glm::dvec2 C = points[tri.v[2]].p;
                
                // Get circumcenter of this triangle
                glm::dvec2 circumcenter_pos = geom2d::tri::circumcenter(A, B, C);
                voronoi_vertices.push_back(circumcenter_pos);
            }
            
            if (!voronoi_vertices.empty()) {
                // Calculate centroid of Voronoi cell
                glm::dvec2 centroid(0.0, 0.0);
                for (const auto& vertex : voronoi_vertices) {
                    centroid += vertex;
                }
                centroid /= static_cast<double>(voronoi_vertices.size());
                
                // Update position
                new_positions[i].p = centroid;
            }
        }
        
        points = std::move(new_positions);
        retriangulate_in_place();
    }
}


void DelaunayTriangulator::edge_flipping_pass() {
    bool improved = true;
    int max_iterations = 10;
    
    while (improved && max_iterations-- > 0) {
        improved = false;
        update_triangle_neighbors();
        
        for (size_t i = 0; i < triangles.size(); ++i) {
            if (!triangles[i].valid) continue;
            
            const Tri& tri = triangles[i];
            for (int j = 0; j < 3; ++j) {
                int neighbor_idx = tri.neighbors[j];
                if (neighbor_idx == -1 || !triangles[neighbor_idx].valid) continue;
                
                Edge shared_edge;
                if (j == 0) shared_edge = Edge(tri.v[0], tri.v[1]);
                else if (j == 1) shared_edge = Edge(tri.v[1], tri.v[2]);
                else shared_edge = Edge(tri.v[2], tri.v[0]);
                

                if (is_constrained(shared_edge.a, shared_edge.b)) continue;

                if (should_flip_edge(static_cast<int>(i), neighbor_idx, shared_edge)) {
                    if (flip_edge(static_cast<int>(i), neighbor_idx, shared_edge)) {
                        improved = true;
                    }
                    break;
                }
            }

            if (improved) break;
        }
    }
    update_triangle_neighbors();
}

void DelaunayTriangulator::refine_min_angle(double min_deg, int max_steiner) {
    int added = 0;

    for (;;) {
        // find worst triangle
        double worst = 1e9;
        int worst_tid = -1;
        for (size_t i=0;i<triangles.size();++i) {
            const Tri& t = triangles[i];
            if (!t.valid) continue;

            // Don’t refine triangles that will be clipped out (outside outer / inside holes).
            if (!active_boundary_loops_.empty() || !active_boundary_loop_.empty()) {
                const auto& A = points[t.v[0]];
                const auto& B = points[t.v[1]];
                const auto& C = points[t.v[2]];
                glm::dvec2 c = geom2d::tri::centroid(A, B, C);
                if (!is_inside_active_domain(c.x, c.y)) continue;
            }

            double a0 = compute_triangle_angle(t,0);
            double a1 = compute_triangle_angle(t,1);
            double a2 = compute_triangle_angle(t,2);
            double mn = std::min({a0,a1,a2});
            if (mn < worst) { worst = mn; worst_tid = (int)i; }
        }
        if (worst_tid < 0 || worst >= min_deg - 1e-6) break;
        if (added >= max_steiner) break;
        const Tri& w = triangles[worst_tid];
        glm::dvec2 A = points[w.v[0]].p;
        glm::dvec2 B = points[w.v[1]].p;
        glm::dvec2 C = points[w.v[2]].p;
        glm::dvec2 cc = geom2d::tri::circumcenter(A,B,C);

        Point2D p(cc.x, cc.y, (int)points.size());
        p.on_boundary = false; // Steiner interior point
        points.push_back(p);

        retriangulate_in_place();

        ++added;
    }
}


bool DelaunayTriangulator::should_flip_edge(int tri1, int tri2, const Edge& edge) {
    const Tri& t1 = triangles[tri1];
    const Tri& t2 = triangles[tri2];
    
    int v1 = -1, v2 = -1;
    
    for (int i = 0; i < 3; ++i) {
        if (t1.v[i] != edge.a && t1.v[i] != edge.b) {
            v1 = t1.v[i];
            break;
        }
    }
    
    for (int i = 0; i < 3; ++i) {
        if (t2.v[i] != edge.a && t2.v[i] != edge.b) {
            v2 = t2.v[i];
            break;
        }
    }
    
    if (v1 == -1 || v2 == -1) return false;
    
    double current_min = std::min({
        compute_triangle_angle(t1, 0),
        compute_triangle_angle(t1, 1), 
        compute_triangle_angle(t1, 2),
        compute_triangle_angle(t2, 0),
        compute_triangle_angle(t2, 1),
        compute_triangle_angle(t2, 2)
    });
    
    // Simulate flipped triangles
    Tri new_t1(v1, v2, edge.a);
    Tri new_t2(v1, v2, edge.b);
    
    double new_min = std::min({
        compute_triangle_angle(new_t1, 0),
        compute_triangle_angle(new_t1, 1),
        compute_triangle_angle(new_t1, 2),
        compute_triangle_angle(new_t2, 0),
        compute_triangle_angle(new_t2, 1),
        compute_triangle_angle(new_t2, 2)
    });
    
    return new_min > current_min + 1e-6;
}

bool DelaunayTriangulator::flip_edge(int tri1, int tri2, const Edge& edge) {
    Tri& t1 = triangles[tri1];
    Tri& t2 = triangles[tri2];
    int v1 = -1, v2 = -1;
    
    for (int i = 0; i < 3; ++i) {
        if (t1.v[i] != edge.a && t1.v[i] != edge.b) {
            v1 = t1.v[i];
            break;
        }
    }
    
    for (int i = 0; i < 3; ++i) {
        if (t2.v[i] != edge.a && t2.v[i] != edge.b) {
            v2 = t2.v[i];
            break;
        }
    }

    if (v1 == -1 || v2 == -1) {
        return false;
    }
    
    t1 = Tri(v1, v2, edge.a, t1.id);
    t2 = Tri(v1, v2, edge.b, t2.id);

    return true;
}

double DelaunayTriangulator::compute_triangle_angle(const Tri& tri, int vertex_idx) const {
    const Point2D& a = points[tri.v[vertex_idx]];
    const Point2D& b = points[tri.v[(vertex_idx + 1) % 3]];
    const Point2D& c = points[tri.v[(vertex_idx + 2) % 3]];
    
    double ab_x = b.x() - a.x(), ab_y = b.y() - a.y();
    double ac_x = c.x() - a.x(), ac_y = c.y() - a.y();
    
    double dot = ab_x * ac_x + ab_y * ac_y;
    double mag_ab = std::sqrt(ab_x * ab_x + ab_y * ab_y);
    double mag_ac = std::sqrt(ac_x * ac_x + ac_y * ac_y);
    
    if (mag_ab < 1e-10 || mag_ac < 1e-10) return 0.0;
    
    double cos_angle = dot / (mag_ab * mag_ac);
    cos_angle = std::max(-1.0, std::min(1.0, cos_angle)); // Clamp to valid range
    
    return glm::degrees(cos_angle);
}

void DelaunayTriangulator::compute_statistics(DelaunayTriangulationResult& result) const {
    if (triangles.empty()) {
        result.min_angle = result.median_angle = result.avg_angle = 0.0;
        result.triangle_count = result.point_count = 0;
        return;
    }
    
    std::vector<double> angles;
    angles.reserve(triangles.size() * 3);
    
    for (const Tri& tri : triangles) {
        if (!tri.valid) continue;
        
        for (int i = 0; i < 3; ++i) {
            angles.push_back(compute_triangle_angle(tri, i));
        }
    }
    
    if (angles.empty()) {
        result.min_angle = result.median_angle = result.avg_angle = 0.0;
        result.triangle_count = result.point_count = 0;
        return;
    }
    
    std::sort(angles.begin(), angles.end());
    
    result.min_angle = angles.front();
    result.median_angle = angles[angles.size() / 2];
    result.avg_angle = std::accumulate(angles.begin(), angles.end(), 0.0) / angles.size();
    result.triangle_count = static_cast<int>(triangles.size());
    result.point_count = static_cast<int>(points.size());
}

bool DelaunayTriangulator::validate_triangulation(const DelaunayTriangulationResult& result, double eps) {
    // Check circumcircle property
    for (const Tri& tri : result.triangles) {
        if (!tri.valid) continue;
        
        glm::dvec2 a = result.points[tri.v[0]].p;
        glm::dvec2 b = result.points[tri.v[1]].p;
        glm::dvec2 c = result.points[tri.v[2]].p;
        
        // Ensure CCW
        if (geom2d::pred::orient_sign(a, b, c) <= 0) return false;
        
        // Check no other point lies inside circumcircle
        for (size_t i = 0; i < result.points.size(); ++i) {
            if (static_cast<int>(i) == tri.v[0] || 
                static_cast<int>(i) == tri.v[1] || 
                static_cast<int>(i) == tri.v[2]) continue;
            
            glm::dvec2 d = result.points[i].p;
            if (geom2d::pred::incircle_ccw(a, b, c, d, eps) > 0) return false;
        }
    }
    
    return true;
}

void DelaunayTriangulator::export_csv(const DelaunayTriangulationResult& result, 
                                     const std::string& nodes_file,
                                     const std::string& triangles_file) {
    // Export nodes
    std::ofstream nodes(nodes_file);
    nodes << "id,x,y,on_boundary,incident_triangles\n";
    
    for (const Point2D& point : result.points) {
        nodes << point.id << "," << point.x() << "," << point.y() << "," 
              << (point.on_boundary ? 1 : 0) << ",\"";
        
        for (size_t i = 0; i < point.incident_triangles.size(); ++i) {
            if (i > 0) nodes << ";";
            nodes << point.incident_triangles[i];
        }
        nodes << "\"\n";
    }
    
    // Export triangles
    std::ofstream triangles_out(triangles_file);
    triangles_out << "id,v0,v1,v2,neighbor0,neighbor1,neighbor2\n";
    
    for (const Tri& tri : result.triangles) {
        if (!tri.valid) continue;
        
        triangles_out << tri.id << "," << tri.v[0] << "," << tri.v[1] << "," << tri.v[2]
                     << "," << tri.neighbors[0] << "," << tri.neighbors[1] << "," << tri.neighbors[2] << "\n";
    }

    std::ofstream edges_csv("edges.csv");
    edges_csv << "id,a,b,tri_left,tri_right,on_boundary,bc_type,bc_value,boundary_tag\n";
    for (size_t i=0;i<result.edges.size();++i) {
        auto& e = result.edges[i];
        edges_csv << i << "," << e.a << "," << e.b << ","
                << e.tri_left << "," << e.tri_right << ","
                << (e.on_boundary?1:0) << ","
                << (int)e.bc.type << "," << e.bc.value << ","
                << e.boundary_tag << "\n";
    }
}




DelaunayTriangulationResult
DelaunayTriangulator::triangulate_with_boundary(const std::vector<Point2D>& input_points,
                                                const std::vector<Point2D>& boundary_poly_ccw)
{
    // 1) merge boundary points into the working set (dedupe by coords)
    std::vector<Point2D> pts = input_points;
    double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
    for (const auto& p : boundary_poly_ccw) {
        xmin = std::min(xmin, p.x());
        xmax = std::max(xmax, p.x());
        ymin = std::min(ymin, p.y());
        ymax = std::max(ymax, p.y());
    }
    const double diag = std::hypot(xmax - xmin, ymax - ymin);
    const double dedupe_tol = std::max(1e-6, diag * 4e-9);
    auto find_or_add = [&](const Point2D& q) {
        for (size_t i = 0; i < pts.size(); ++i) {
            if (std::abs(pts[i].x() - q.x()) < dedupe_tol &&
                std::abs(pts[i].y() - q.y()) < dedupe_tol) return (int)i;
        }
        pts.emplace_back(q.x(), q.y(), (int)pts.size());
        return (int)pts.size() - 1;
    };

    std::vector<int> poly_idx;
    poly_idx.reserve(boundary_poly_ccw.size());
    for (const auto& p : boundary_poly_ccw) poly_idx.push_back(find_or_add(p));

    for (int id : poly_idx) {
        if (id >= 0 && id < (int)pts.size()) pts[id].on_boundary = true;
    }

    active_boundary_loop_ = poly_idx;
    active_boundary_loops_.clear();
    active_boundary_loops_.push_back(poly_idx);
    rebuild_active_loop_bboxes_(pts);

    constrained_keys_.clear();
    constraints_.clear();

    DelaunayTriangulationResult R = triangulate(pts);

    if (points_on_circle(pts)) {
        LOGT_INFO(LogGeometry, "Skipping constraint recovery for concyclic case.");
        return R;
    }

    build_constraints_from_loops({ poly_idx });
    enforce_constraints();

    build_adjacency();

    {
        DelaunayTriangulationResult constrained;
        constrained.points = points;
        constrained.triangles.reserve(triangles.size());
        for (const auto& t : triangles) if (t.valid) constrained.triangles.push_back(t);

        constrained.tri2vert.resize(constrained.triangles.size());
        constrained.tri_neighbors.resize(constrained.triangles.size());
        for (size_t i = 0; i < constrained.triangles.size(); ++i) {
            const auto& t = constrained.triangles[i];
            constrained.tri2vert[i] = { t.v[0], t.v[1], t.v[2] };
            constrained.tri_neighbors[i] = { t.neighbors[0], t.neighbors[1], t.neighbors[2] };
        }

        constrained.vert2tri.resize(constrained.points.size());
        for (size_t i = 0; i < constrained.points.size(); ++i) {
            constrained.vert2tri[i] = points[i].incident_triangles;
        }

        constrained.edges = edges_cache_;
        constrained.tri2edge = tri2edge_cache_;
        R = std::move(constrained);
    }

    // 3) mark boundary nodes + make boundary edge list for the result
    for (int id : poly_idx)
        if (R.valid_point(id)) R.points[id].on_boundary = true;

    R.boundary_edges.clear();
    if (poly_idx.size() >= 2) {
        for (size_t i = 0; i < poly_idx.size(); ++i) {
            int a = poly_idx[i], b = poly_idx[(i + 1) % poly_idx.size()];
            Edge e(a, b); e.on_boundary = true;
            R.boundary_edges.push_back(e);
        }
    }

    // 4) optional clipping: keep triangles whose centroid is inside the polygon
    if (poly_idx.size() >= 3) {
        std::vector<Tri> kept;
        kept.reserve(R.triangles.size());
        for (const auto& t : R.triangles) {
            const auto& A = R.points[t.v[0]];
            const auto& B = R.points[t.v[1]];
            const auto& C = R.points[t.v[2]];
            glm::dvec2 c = geom2d::tri::centroid(A, B, C);
            if (pointInPoly(R.points, poly_idx, c.x, c.y))
                kept.push_back(t);
        }
        R.triangles.swap(kept);
        build_edges_for_result(R);
        compute_statistics(R);
        remove_dangling_vertices(R);
    }

    active_boundary_loop_.clear();
    active_boundary_loops_.clear();
    active_loop_bboxes_.clear();

    return R;
}

DelaunayTriangulationResult
DelaunayTriangulator::triangulate_polygon(const std::vector<Point2D>& polygon_ccw)
{
    // polygon vertices serve as both input points and boundary
    return triangulate_with_boundary(polygon_ccw, polygon_ccw);
}

void DelaunayTriangulator::set_density_function(std::shared_ptr<DensityFunction> f) {
    density_fn_ = std::move(f);
}

void DelaunayTriangulator::refine_to_density() {
    if (!config.enable_sizing_refinement || !density_fn_) return;

    build_adjacency();

    using clock = std::chrono::steady_clock;
    const auto t_refine_start = clock::now();

    int added = 0;
    double total_scan_ms = 0.0;
    double total_retri_ms = 0.0;

    // Avoid creating nearly-duplicate points which can destabilize constraint recovery.
    const double min_sep = std::max(1e-12, config.epsilon * 50.0);
    const double min_sep2 = min_sep * min_sep;

    for (;;) {
        const auto t_scan0 = clock::now();

        double best_ratio = -1.0;
        glm::dvec2 mid(0.0);
        bool best_is_boundary_segment = false;

        for (const EdgeInfo& e : edges_cache_) {
            if (e.a < 0 || e.b < 0) continue;
            if (!e.valid_vertices(points.size())) continue;

            // Critical: never split constrained edges by inserting a point on them.
            // Doing so makes the original constraint (a,b) impossible to recover.
            if (is_constrained(e.a, e.b)) continue;

            const auto& A = points[e.a];
            const auto& B = points[e.b];

            double L = std::hypot(B.x() - A.x(), B.y() - A.y());
            double mx = 0.5 * (A.x() + B.x()), my = 0.5 * (A.y() + B.y());

            // For porous domains, ignore edges that lie outside the domain or inside holes.
            if ((!active_boundary_loops_.empty() || !active_boundary_loop_.empty()) &&
                !is_inside_active_domain(mx, my)) {
                continue;
            }

            double h  = std::max(1e-9, density_fn_->edge_length_at(mx, my));
            double r  = L / h;

            if (r > best_ratio) {
                best_ratio = r;
                mid = {mx, my};
                best_is_boundary_segment = false;
            }
        }

        const auto t_scan1 = clock::now();
        total_scan_ms += std::chrono::duration<double, std::milli>(t_scan1 - t_scan0).count();

        if (best_ratio <= config.density_refine_threshold) break;
        if (added >= config.refine_sizing_max_steiner) break;

        // Skip if the candidate is too close to an existing vertex.
        bool too_close = false;
        for (const auto& p0 : points) {
            const double dx = p0.x() - mid.x;
            const double dy = p0.y() - mid.y;
            if (dx * dx + dy * dy <= min_sep2) { too_close = true; break; }
        }
        if (too_close) break;

        Point2D p(mid.x, mid.y, (int)points.size());
        p.on_boundary = best_is_boundary_segment;
        points.push_back(p);

        const auto t_retri0 = clock::now();

        retriangulate_in_place_unconstrained_();

        const auto t_retri1 = clock::now();
        total_retri_ms += std::chrono::duration<double, std::milli>(t_retri1 - t_retri0).count();

        ++added;
    }

    const auto t_refine_end = clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(t_refine_end - t_refine_start).count();
    LOGT_DEBUG(LogGeometry,
               "[CPU] refine_to_density: added=%d scan=%.2fms retri=%.2fms total=%.2fms",
               added, total_scan_ms, total_retri_ms, total_ms);

    if (!active_boundary_loops_.empty() || !active_boundary_loop_.empty()) {
        enforce_active_loop_constraints_if_any();
        build_adjacency();
    }
}

DelaunayTriangulationResult
DelaunayTriangulator::triangulate_with_boundaries(
    const std::vector<Point2D>& input_points,
    const std::vector<std::vector<Point2D>>& loops_ccw_outer_cw_holes)
{
    DelaunayTriangulationResult R;

    if (loops_ccw_outer_cw_holes.empty()) {
        return triangulate(input_points);
    }

    std::vector<Point2D> pts = input_points;

    double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
    for (const auto& loop : loops_ccw_outer_cw_holes) {
        for (const auto& p : loop) {
            xmin = std::min(xmin, p.x());
            xmax = std::max(xmax, p.x());
            ymin = std::min(ymin, p.y());
            ymax = std::max(ymax, p.y());
        }
    }
    const double diag = std::hypot(xmax - xmin, ymax - ymin);
    const double weld_eps = std::max(1e-6, diag * 1e-8);
    const double snap_step = weld_eps;
    boundary_weld_eps_ = weld_eps;
    boundary_intersect_eps_ = std::max(config.epsilon, weld_eps * 0.5);

    auto snap = [&](double v) {
        if (!(snap_step > 0.0)) return v;
        return std::round(v / snap_step) * snap_step;
    };

    const double dedupe_tol = weld_eps;
    auto find_or_add = [&](const Point2D& q) {
        const double qx = snap(q.x());
        const double qy = snap(q.y());
        for (size_t i = 0; i < pts.size(); ++i) {
            if (std::abs(pts[i].x() - qx) < dedupe_tol &&
                std::abs(pts[i].y() - qy) < dedupe_tol)
            {
                return (int)i;
            }
        }
        pts.emplace_back(qx, qy, (int)pts.size());
        return (int)pts.size() - 1;
    };

    std::vector<std::vector<int>> loop_idx(loops_ccw_outer_cw_holes.size());

    for (size_t L = 0; L < loops_ccw_outer_cw_holes.size(); ++L) {
        std::vector<Point2D> loop = loops_ccw_outer_cw_holes[L];

        auto signed_area = [](const std::vector<Point2D>& poly) {
            double a = 0.0;
            if (poly.size() < 2) return 0.0;
            for (size_t i = 0; i < poly.size(); ++i) {
                const auto& p = poly[i];
                const auto& q = poly[(i + 1) % poly.size()];
                a += p.x() * q.y() - p.y() * q.x();
            }
            return 0.5 * a;
        };

        double A = signed_area(loop);
        if (L == 0) {
            if (A < 0.0) std::reverse(loop.begin(), loop.end());
        } else {
            if (A > 0.0) std::reverse(loop.begin(), loop.end());
        }

        loop_idx[L].reserve(loop.size());
        for (const auto& p : loop) {
            int id = find_or_add(p);
            loop_idx[L].push_back(id);
            pts[id].on_boundary = true;
        }
    }

    {
        double shortest_hole_edge = 1e300;
        for (std::size_t li = 1; li < loop_idx.size(); ++li) {
            const auto& L = loop_idx[li];
            for (std::size_t ei = 0; ei < L.size(); ++ei) {
                const int ai = L[ei], bi = L[(ei + 1) % L.size()];
                const double len = std::hypot(pts[bi].x() - pts[ai].x(),
                                              pts[bi].y() - pts[ai].y());
                if (len > 1e-12) shortest_hole_edge = std::min(shortest_hole_edge, len);
            }
        }
        if (shortest_hole_edge > 1e200) shortest_hole_edge = 1.0; // fallback

        const double max_edge = std::max(shortest_hole_edge * 0.9, 1.0);

        auto sanitize_loop = [&](std::vector<int>& L) {
            if (L.size() < 3) return;
            const double min_len = weld_eps;

            for (int pass = 0; pass < 4; ++pass) {
                bool changed = false;
                if (L.size() < 3) break;

                for (std::size_t i = 0; i < L.size() && L.size() >= 3; ++i) {
                    const std::size_t j = (i + 1) % L.size();
                    if (L[i] == L[j]) {
                        L.erase(L.begin() + (std::ptrdiff_t)j);
                        changed = true;
                        break;
                    }
                }
                if (changed) continue;

                for (std::size_t i = 0; i < L.size() && L.size() >= 3; ++i) {
                    const std::size_t j = (i + 1) % L.size();
                    const int a = L[i];
                    const int b = L[j];
                    if (!detail::valid_index(a, pts.size()) || !detail::valid_index(b, pts.size())) continue;
                    const double len = std::hypot(pts[(std::size_t)b].x() - pts[(std::size_t)a].x(),
                                                  pts[(std::size_t)b].y() - pts[(std::size_t)a].y());
                    if (len < min_len) {
                        L.erase(L.begin() + (std::ptrdiff_t)j);
                        changed = true;
                        break;
                    }
                }
                if (!changed) break;
            }
        };

        for (auto& L : loop_idx) {
            std::vector<int> newL;
            newL.reserve(L.size() * 2);
            for (std::size_t ei = 0; ei < L.size(); ++ei) {
                const int ai = L[ei];
                const int bi = L[(ei + 1) % L.size()];
                newL.push_back(ai);

                const double dx = pts[bi].x() - pts[ai].x();
                const double dy = pts[bi].y() - pts[ai].y();
                const double len = std::hypot(dx, dy);
                if (len > max_edge) {
                    const int nseg = (int)std::ceil(len / max_edge);
                    for (int s = 1; s < nseg; ++s) {
                        const double t = (double)s / (double)nseg;
                        const double x = pts[ai].x() + t * dx;
                        const double y = pts[ai].y() + t * dy;
                        // Dedupe: avoid creating coincident vertices with different ids.
                        Point2D q(x, y, -1);
                        const int nid = find_or_add(q);
                        if (nid >= 0 && (std::size_t)nid < pts.size()) pts[(std::size_t)nid].on_boundary = true;
                        newL.push_back(nid);
                    }
                }
            }
            L = std::move(newL);
            sanitize_loop(L);

            for (int id : L) {
                if (!detail::valid_index(id, pts.size())) continue;
                if (!pts[(std::size_t)id].on_boundary) continue;
                pts[(std::size_t)id].p.x = snap(pts[(std::size_t)id].p.x);
                pts[(std::size_t)id].p.y = snap(pts[(std::size_t)id].p.y);
            }
            sanitize_loop(L);
        }
    }

    // Make loops visible to density refinement
    active_boundary_loop_.clear();
    active_boundary_loops_.clear();
    if (!loop_idx.empty()) {
        active_boundary_loop_ = loop_idx[0];
        active_boundary_loops_ = loop_idx;
    }
    rebuild_active_loop_bboxes_(pts);

    R = triangulate(pts);

    build_constraints_from_loops(loop_idx);
    enforce_constraints();
    build_adjacency();

    // Rebuild from constrained state
    {
        DelaunayTriangulationResult constrained;
        constrained.min_angle = R.min_angle;
        constrained.median_angle = R.median_angle;
        constrained.avg_angle = R.avg_angle;
        constrained.triangle_count = R.triangle_count;
        constrained.point_count = R.points.size();
        constrained.points = points;
        constrained.triangles.reserve(triangles.size());
        for (const auto& t : triangles) if (t.valid) constrained.triangles.push_back(t);

        constrained.tri2vert.resize(constrained.triangles.size());
        constrained.tri_neighbors.resize(constrained.triangles.size());
        for (size_t i = 0; i < constrained.triangles.size(); ++i) {
            const auto& t = constrained.triangles[i];
            constrained.tri2vert[i] = { t.v[0], t.v[1], t.v[2] };
            constrained.tri_neighbors[i] = { t.neighbors[0], t.neighbors[1], t.neighbors[2] };
        }

        constrained.vert2tri.resize(constrained.points.size());
        for (size_t i = 0; i < constrained.points.size(); ++i) {
            constrained.vert2tri[i] = points[i].incident_triangles;
        }

        constrained.edges = edges_cache_;
        constrained.tri2edge = tri2edge_cache_;
        R = std::move(constrained);
    }

    // mark boundary points in result
    for (const auto& L : loop_idx) {
        for (int id : L) {
            if (R.valid_point(id)) {
                R.points[id].on_boundary = true;
            }
        }
    }

    // boundary edges for all loops
    R.boundary_edges.clear();
    for (const auto& L : loop_idx) {
        if (L.size() < 2) continue;
        for (size_t i = 0; i < L.size(); ++i) {
            int a = L[i], b = L[(i + 1) % L.size()];
            Edge e(a, b);
            e.on_boundary = true;
            R.boundary_edges.push_back(e);
        }
    }

    if (!loop_idx.empty() && loop_idx[0].size() >= 3) {

        struct HoleSeg { glm::dvec2 a, b; };
        struct HoleInfo {
            std::vector<HoleSeg> segs;
            double xmin =  1e300, xmax = -1e300;
            double ymin =  1e300, ymax = -1e300;
        };
        std::vector<HoleInfo> holes_info;
        holes_info.reserve(loop_idx.size());
        for (std::size_t h = 1; h < loop_idx.size(); ++h) {
            const auto& L = loop_idx[h];
            if (L.size() < 3) continue;
            HoleInfo hi;
            for (std::size_t ei = 0; ei < L.size(); ++ei) {
                const int ai = L[ei], bi = L[(ei + 1) % L.size()];
                const auto& pa = R.points[ai];
                const auto& pb = R.points[bi];
                hi.segs.push_back({pa.p, pb.p});
                hi.xmin = std::min({hi.xmin, pa.x(), pb.x()});
                hi.xmax = std::max({hi.xmax, pa.x(), pb.x()});
                hi.ymin = std::min({hi.ymin, pa.y(), pb.y()});
                hi.ymax = std::max({hi.ymax, pa.y(), pb.y()});
            }
            holes_info.push_back(std::move(hi));
        }

        auto inside_outer = [&](double x, double y) {
            if (!this->active_loop_bboxes_.empty() && !this->active_loop_bboxes_[0].contains(x, y)) return false;
            return pointInPoly(R.points, loop_idx[0], x, y);
        };

        const double clip_eps = std::max(config.epsilon, boundary_intersect_eps_);
        auto edge_crosses_hole = [&](const glm::dvec2& P, const glm::dvec2& Q) -> bool {
            const double exmin = std::min(P.x, Q.x);
            const double exmax = std::max(P.x, Q.x);
            const double eymin = std::min(P.y, Q.y);
            const double eymax = std::max(P.y, Q.y);
            for (const auto& hi : holes_info) {
                if (exmax < hi.xmin || exmin > hi.xmax) continue;
                if (eymax < hi.ymin || eymin > hi.ymax) continue;
                for (const auto& s : hi.segs) {
                    if (segments_intersect(P, Q, s.a, s.b, clip_eps))
                        return true;
                }
            }
            return false;
        };

        std::vector<Tri> kept;
        kept.reserve(R.triangles.size());
        for (const auto& t : R.triangles) {
            const auto& A = R.points[t.v[0]];
            const auto& B = R.points[t.v[1]];
            const auto& C = R.points[t.v[2]];

            const glm::dvec2 c = geom2d::tri::centroid(A, B, C);
            if (!inside_outer(c.x, c.y)) continue;

            bool in_hole = false;
            for (std::size_t hi = 0; hi < holes_info.size(); ++hi) {
                const auto& hole = holes_info[hi];
                if (!hole.segs.empty()) {
                    if (c.x < hole.xmin || c.x > hole.xmax) continue;
                    if (c.y < hole.ymin || c.y > hole.ymax) continue;
                }
                const std::size_t loop_h = hi + 1;
                if (loop_h < loop_idx.size() && loop_idx[loop_h].size() >= 3) {
                    if (pointInPoly(R.points, loop_idx[loop_h], c.x, c.y)) { in_hole = true; break; }
                }
            }
            if (in_hole) continue;

            if (edge_crosses_hole(A.p, B.p)) continue;
            if (edge_crosses_hole(B.p, C.p)) continue;
            if (edge_crosses_hole(C.p, A.p)) continue;

            kept.push_back(t);
        }
        R.triangles.swap(kept);
        build_edges_for_result(R);
        remove_dangling_vertices(R);
    }

    active_boundary_loop_.clear();
    active_boundary_loops_.clear();
    active_loop_bboxes_.clear();

    return R;
}

bool DelaunayTriangulator::edge_exists(int a, int b) const {
    const auto k = detail::pack_edge(a, b);
    return edge_index_cache_.find(k) != edge_index_cache_.end();
}

Edge DelaunayTriangulator::find_first_intersecting_edge(int a, int b) const {
    const glm::dvec2 A = points[a].p;
    const glm::dvec2 B = points[b].p;

    for (const EdgeInfo& e : edges_cache_) {
        if (!e.valid_vertices(points.size())) continue;
        const int u = e.a, v = e.b;
        if (u < 0 || v < 0) continue;
        if (share_endpoint(a, b, u, v)) continue;

        const glm::dvec2 C = points[u].p;
        const glm::dvec2 D = points[v].p;

        if (segments_intersect(A, B, C, D, config.epsilon)) {
            return Edge(u, v);
        }
    }
    return Edge(-1, -1);
}


bool DelaunayTriangulator::flip_edge_if_possible(int ea, int eb) {
    const auto k = detail::pack_edge(ea, eb);
    auto it = edge_index_cache_.find(k);
    if (it == edge_index_cache_.end()) return false;

    const int eid = it->second;
    if (eid < 0 || (size_t)eid >= edges_cache_.size()) return false;

    const auto& ei = edges_cache_[eid];
    if (ei.tri_left < 0 || ei.tri_right < 0) return false; // on boundary?
    if (is_constrained(ea, eb)) return false;

    if (!flip_edge(ei.tri_left, ei.tri_right, Edge(ea, eb))) return false;

    auto& t1 = triangles[ei.tri_left];
    auto& t2 = triangles[ei.tri_right];

    if (t1.v[0] < 0 || t1.v[1] < 0 || t1.v[2] < 0 ||
        t2.v[0] < 0 || t2.v[1] < 0 || t2.v[2] < 0) {
        return false;
    }

    auto fix_ccw = [&](Tri& t) {
        glm::dvec2 A = points[t.v[0]].p;
        glm::dvec2 B = points[t.v[1]].p;
        glm::dvec2 C = points[t.v[2]].p;
        if (geom2d::pred::orient_sign(A, B, C) < 0) std::swap(t.v[1], t.v[2]);
    };
    fix_ccw(t1);
    fix_ccw(t2);

    return true;
}


int DelaunayTriangulator::insert_boundary_point_on_loop_edge_(int a, int b) {
    if (a < 0 || b < 0) return -1;
    if (a >= (int)points.size() || b >= (int)points.size()) return -1;

    const glm::dvec2 A = points[a].p;
    const glm::dvec2 B = points[b].p;
    const glm::dvec2 AB = B - A;
    const double ab2 = AB.x * AB.x + AB.y * AB.y;
    const double min_sep = std::max(1e-6, boundary_weld_eps_);
    if (ab2 < (min_sep * 4.0) * (min_sep * 4.0)) return -1;

    // Find the owning loop edge first; never reuse global vertices here because
    // that can accidentally glue different holes together.
    std::vector<int>* owner_loop = nullptr;
    std::size_t owner_i = 0;
    for (auto& L : active_boundary_loops_) {
        if (L.size() < 2) continue;
        for (std::size_t i = 0; i < L.size(); ++i) {
            const int u = L[i];
            const int v = L[(i + 1) % L.size()];
            if ((u == a && v == b) || (u == b && v == a)) {
                owner_loop = &L;
                owner_i = i;
                break;
            }
        }
        if (owner_loop) break;
    }
    if (!owner_loop) return -1;

    const double ts[3] = {0.5, 1.0 / 3.0, 2.0 / 3.0};
    for (double t : ts) {
        glm::dvec2 P = A + t * AB;

        // Reject points too close to either endpoint (would return a/b and cause
        // split logging like (a,a)+(a,b) and loop corruption).
        const double da2 = (P.x - A.x) * (P.x - A.x) + (P.y - A.y) * (P.y - A.y);
        const double db2 = (P.x - B.x) * (P.x - B.x) + (P.y - B.y) * (P.y - B.y);
        if (da2 <= (min_sep * min_sep) || db2 <= (min_sep * min_sep)) continue;

        // Snap to the same grid used by boundary input.
        const double step = std::max(1e-6, boundary_weld_eps_);
        P.x = std::round(P.x / step) * step;
        P.y = std::round(P.y / step) * step;

        // Ensure snapping didn't collapse back onto endpoints.
        const double da2s = (P.x - A.x) * (P.x - A.x) + (P.y - A.y) * (P.y - A.y);
        const double db2s = (P.x - B.x) * (P.x - B.x) + (P.y - B.y) * (P.y - B.y);
        if (da2s <= (min_sep * min_sep) || db2s <= (min_sep * min_sep)) continue;

        const int new_id = (int)points.size();
        Point2D np(P.x, P.y, new_id);
        np.on_boundary = true;
        points.push_back(np);

        owner_loop->insert(owner_loop->begin() + (std::ptrdiff_t)owner_i + 1, new_id);
        if (!active_boundary_loops_.empty()) active_boundary_loop_ = active_boundary_loops_[0];
        rebuild_active_loop_bboxes_(points);

        LOGT_WARN(LogGeometry,
                  "Inserted boundary point %d on loop edge (%d,%d) to aid constraint recovery.",
                  new_id, a, b);
        return new_id;
    }

    return -1;
}


bool DelaunayTriangulator::recover_constraint(int a, int b) {
    // Assumes adjacency + edge caches are already built by the caller.
    // NOTE: Do NOT mutate constraints_ here. constraints_ is the fixed list of
    // boundary constraints we want to enforce; adding "helper" constraints while
    // iterating can create constraints that are not loop edges and cannot be
    // split/patched, leading to repeated failures and boundary artifacts.
    if (edge_exists(a, b)) return true;

    if (int c = find_blocking_vertex_on_segment(a, b); c != -1) {
        // Only split at a boundary vertex; splitting at arbitrary interior
        // vertices would introduce non-boundary constraints.
        if (c >= 0 && c < (int)points.size() && points[c].on_boundary) {
            return recover_constraint(a, c) && recover_constraint(c, b);
        }
    }

    // IMPORTANT: each iteration scans all edges to find a flippable intersection,
    // so letting this scale with edge count can effectively hang for large meshes
    // (e.g. carpets with ~1000 holes). Prefer failing fast and letting the caller
    // split the boundary edge + retriangulate.
    const double seg_eps = std::max(config.epsilon, boundary_intersect_eps_);

    int max_iter = (int)std::sqrt((double)edges_cache_.size()) * 2;
    if (max_iter < 64) max_iter = 64;
    if (max_iter > 256) max_iter = 256;
    for (int it = 0; it < max_iter; ++it) {
        if (edge_exists(a, b)) return true;

        // Find an intersecting edge we are allowed to flip.
        // The first intersecting edge might itself be constrained or unflippable;
        // try other intersecting edges before giving up.
        const glm::dvec2 A = points[a].p;
        const glm::dvec2 B = points[b].p;

        bool flipped = false;
        for (const EdgeInfo& e : edges_cache_) {
            if (!e.valid_vertices(points.size())) continue;
            
            const int u = e.a;
            const int v = e.b;
            if (share_endpoint(a, b, u, v)) continue;

            const glm::dvec2 C = points[u].p;
            const glm::dvec2 D = points[v].p;
            if (!segments_intersect(A, B, C, D, seg_eps)) continue;

            if (is_constrained(u, v)) continue;
            if (e.tri_left < 0 || e.tri_right < 0) continue; // boundary/unflippable

            if (flip_edge_if_possible(u, v)) {
                flipped = true;
                break;
            }
        }

        if (!flipped) return false;

        build_adjacency();
    }

    return false;
}

void DelaunayTriangulator::constrained_delaunay_flip_pass() {
    bool changed = true;
    int guard = 0;

    while (changed && guard++ < 50) {
        changed = false;
        build_adjacency();

        for (const auto& ei : edges_cache_) {
            if (ei.tri_left < 0 || ei.tri_right < 0) continue; // boundary edge
            if (is_constrained(ei.a, ei.b)) continue;

            const Tri& tL = triangles[ei.tri_left];
            const Tri& tR = triangles[ei.tri_right];

            const int a = ei.a;
            const int b = ei.b;
            const int c = opposite_vertex(tL, a, b);
            const int d = opposite_vertex(tR, a, b);
            if (c < 0 || d < 0) continue;

            glm::dvec2 A = points[a].p;
            glm::dvec2 B = points[b].p;
            glm::dvec2 C = points[c].p;
            glm::dvec2 D = points[d].p;

            // Ensure (A,B,C) is CCW for incircle test
            if (geom2d::pred::orient_sign(A, B, C) < 0) std::swap(B, C);

            // If D is inside circumcircle of triangle ABC, edge (a,b) is illegal => flip
            if (geom2d::pred::incircle_ccw_scaled_strict(A, B, C, D) > 0) {
                if (!flip_edge(ei.tri_left, ei.tri_right, Edge(a, b))) continue;

                // fix orientation
                auto& nL = triangles[ei.tri_left];
                auto& nR = triangles[ei.tri_right];

                auto fix_ccw = [&](Tri& t) {
                    glm::dvec2 p0 = points[t.v[0]].p;
                    glm::dvec2 p1 = points[t.v[1]].p;
                    glm::dvec2 p2 = points[t.v[2]].p;
                    if (geom2d::pred::orient_sign(p0, p1, p2) < 0) std::swap(t.v[1], t.v[2]);
                };
                fix_ccw(nL);
                fix_ccw(nR);

                changed = true;
                break;
            }
        }

        if (changed) {
            continue;
        }
    }
}

void DelaunayTriangulator::build_constraints_from_loops(const std::vector<std::vector<int>>& loops) {
    constrained_keys_.clear();
    constraints_.clear();

    const double min_len = std::max(1e-6, boundary_weld_eps_ * 4.0);

    for (const auto& L : loops) {
        if (L.size() < 2) continue;
        for (size_t i = 0; i < L.size(); ++i) {
            const int a = L[i];
            const int b = L[(i + 1) % L.size()];
            if (a == b) continue;
            if (!detail::valid_index(a, points.size()) || !detail::valid_index(b, points.size())) continue;
            const double len = std::hypot(points[(std::size_t)b].x() - points[(std::size_t)a].x(),
                                          points[(std::size_t)b].y() - points[(std::size_t)a].y());
            if (len < min_len) continue;
            add_constraint(a, b);
        }
    }
}

void DelaunayTriangulator::enforce_constraints() {
    static bool dumped_failure_diagnostics = false;
    int failure_count = 0;

    std::size_t split_count = 0;
    const std::size_t max_splits = 64;

    // Track constraint pairs that have already failed-and-been-skipped so we
    // never infinitely restart on the same unsplittable edge.
    std::unordered_set<detail::EdgeKey, detail::U64Hash> permanently_failed;

    // Compute a scale-aware boundary epsilon from the current boundary points.
    {
        double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
        bool any = false;
        for (const auto& p : points) {
            if (!p.on_boundary) continue;
            any = true;
            xmin = std::min(xmin, p.x());
            xmax = std::max(xmax, p.x());
            ymin = std::min(ymin, p.y());
            ymax = std::max(ymax, p.y());
        }
        if (any) {
            const double diag = std::hypot(xmax - xmin, ymax - ymin);
            boundary_weld_eps_ = std::max(1e-6, diag * 1e-8);
            boundary_intersect_eps_ = std::max(config.epsilon, boundary_weld_eps_ * 0.5);
        }
    }

    // Build caches once; recover_constraint() will rebuild only when it flips.
    build_adjacency();

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    std::size_t i = 0;
    while (i < constraints_.size()) {
        const auto [a, b] = constraints_[i];

        const double degenerate_len = std::max(1e-6, boundary_weld_eps_ * 4.0);

        const std::size_t total = constraints_.size();
        const std::size_t log_every = (total >= 2000 ? 200 : (total >= 500 ? 100 : 0));

        if (log_every > 0 && (i % log_every) == 0) {
            const auto t1 = clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            LOGT_DEBUG(LogGeometry,
                       "enforce_constraints: %zu/%zu (%.1f%%) elapsed=%.1fms",
                       i, total, (total > 0 ? (100.0 * (double)i / (double)total) : 100.0), ms);
        }

        // Skip constraints already known to be unrecoverable.
        if (permanently_failed.count(detail::pack_edge(a, b))) { ++i; continue; }

        if (!recover_constraint(a, b)) {
            // Degenerate constraint: collapse it in the owning loop.
            if (detail::valid_index(a, points.size()) && detail::valid_index(b, points.size())) {
                const glm::dvec2 A = points[a].p;
                const glm::dvec2 B = points[b].p;
                const double seg_len = std::hypot(B.x - A.x, B.y - A.y);
                if (seg_len < degenerate_len) {
                    bool collapsed = false;
                    for (auto& L : active_boundary_loops_) {
                        if (L.size() <= 3) continue;
                        const std::size_t n = L.size();
                        for (std::size_t k = 0; k < n; ++k) {
                            const int u = L[k];
                            const int v = L[(k + 1) % n];
                            if ((u == a && v == b) || (u == b && v == a)) {
                                const std::size_t rm = (k + 1) % n;
                                L.erase(L.begin() + (std::ptrdiff_t)rm);
                                collapsed = true;
                                break;
                            }
                        }
                        if (collapsed) break;
                    }

                    if (collapsed) {
                        if (!active_boundary_loops_.empty()) active_boundary_loop_ = active_boundary_loops_[0];
                        rebuild_active_loop_bboxes_(points);

                        LOGT_WARN(LogGeometry,
                                  "Collapsed degenerate boundary edge (%d,%d) (len=%.3g) and restarting enforcement.",
                                  a, b, seg_len);
                        retriangulate_in_place_unconstrained_();
                        build_constraints_from_loops(active_boundary_loops_);
                        build_adjacency();
                        i = 0;
                        continue;
                    }
                }
            }

            // If this is a hard boundary edge, try to split it.
            if (split_count < max_splits) {
                const int new_id = insert_boundary_point_on_loop_edge_(a, b);
                if (new_id != -1) {
                    ++split_count;
                    LOGT_WARN(LogGeometry,
                              "Restarting constraint enforcement after splitting (%d,%d) -> (%d,%d)+(%d,%d) (split=%zu).",
                              a, b, a, new_id, new_id, b, split_count);

                    retriangulate_in_place_unconstrained_();
                    build_constraints_from_loops(active_boundary_loops_);
                    build_adjacency();
                    i = 0;
                    continue;
                }
            }

            // Mark this constraint as permanently failed so we don't trigger
            // infinite restarts if a later edge splits + restarts from 0.
            permanently_failed.insert(detail::pack_edge(a, b));
            LOGT_ERROR(LogGeometry, "Failed to recover constraint edge (%d,%d).", a, b);

            if (!dumped_failure_diagnostics && failure_count < 12) {
                ++failure_count;

                if (a >= 0 && b >= 0 && a < (int)points.size() && b < (int)points.size()) {
                    const glm::dvec2 A = points[a].p;
                    const glm::dvec2 B = points[b].p;
                    const double seg_len = std::hypot(B.x - A.x, B.y - A.y);

                    build_adjacency();
                    int intersecting_edges = 0;
                    for (const EdgeInfo& e : edges_cache_) {
                        if (!e.valid_vertices(points.size())) continue;
                        if (share_endpoint(a, b, e.a, e.b)) continue;
                        const glm::dvec2 C = points[e.a].p;
                        const glm::dvec2 D = points[e.b].p;
                        if (segments_intersect(A, B, C, D, config.epsilon)) {
                            ++intersecting_edges;
                        }
                    }

                    const int c = find_blocking_vertex_on_segment(a, b);

                    LOGT_WARN(LogGeometry,
                              "Constraint diagnostic: (%d,%d) len=%.6g A=(%.6g,%.6g) B=(%.6g,%.6g) A.boundary=%d B.boundary=%d intersects=%d blocking=%d",
                              a, b, seg_len,
                              A.x, A.y, B.x, B.y,
                              points[a].on_boundary ? 1 : 0,
                              points[b].on_boundary ? 1 : 0,
                              intersecting_edges,
                              c);
                } else {
                    LOGT_WARN(LogGeometry,
                              "Constraint diagnostic: (%d,%d) out of range (point count: %zu)",
                              a, b, points.size());
                }
            }
        }

        ++i;
    }

    if (!dumped_failure_diagnostics && failure_count > 0) {
        dumped_failure_diagnostics = true;
    }

    constrained_delaunay_flip_pass();

    build_adjacency();
}

DelaunayTriangulationResult DelaunayTriangulator::build_result_from_state() const {
        DelaunayTriangulationResult R;
        R.points = points;
        R.triangles.reserve(triangles.size());
        for (const auto& t : triangles) if (t.valid) R.triangles.push_back(t);

        R.tri2vert.resize(R.triangles.size());
        R.tri_neighbors.resize(R.triangles.size());
        for (size_t i = 0; i < R.triangles.size(); ++i) {
            const auto& t = R.triangles[i];
            R.tri2vert[i] = { t.v[0], t.v[1], t.v[2] };
            R.tri_neighbors[i] = { t.neighbors[0], t.neighbors[1], t.neighbors[2] };
        }

        R.vert2tri.assign(R.points.size(), {});
        for (size_t i = 0; i < R.triangles.size(); ++i) {
            const auto& t = R.triangles[i];
            if (t.valid_vertex(0, R.points.size())) R.vert2tri[t.v[0]].push_back((int)i);
            if (t.valid_vertex(1, R.points.size())) R.vert2tri[t.v[1]].push_back((int)i);
            if (t.valid_vertex(2, R.points.size())) R.vert2tri[t.v[2]].push_back((int)i);
        }

        build_edges_for_result(R);
        compute_statistics(R);
        remove_dangling_vertices(R);
        return R;
}


DelaunayTriangulationResult
DelaunayTriangulator::triangulate_circle(const std::vector<Point2D>& input_points)
{
    DelaunayTriangulationResult R;
    if (input_points.size() < 3) {
        R.points = input_points;
        return R;
    }

    R.points = input_points;

    glm::dvec2 center(0.0);
    for (auto& p : R.points) center += p.p;
    center /= static_cast<double>(R.points.size());

    std::vector<int> idx(R.points.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int i, int j){
        double ai = std::atan2(R.points[i].y() - center.y, R.points[i].x() - center.x);
        double aj = std::atan2(R.points[j].y() - center.y, R.points[j].x() - center.x);
        return ai < aj;
    });

    int n = static_cast<int>(idx.size());
    for (int i = 1; i < n - 1; ++i) {
        Tri t(idx[0], idx[i], idx[i + 1], (int)R.triangles.size());
        R.triangles.push_back(t);
    }

    build_edges_for_result(R);
    compute_statistics(R);
    remove_dangling_vertices(R);

    points = R.points;
    triangles = R.triangles;
    boundary_edges = R.boundary_edges;
    active_boundary_loop_.clear();
    active_boundary_loops_.clear();
    active_loop_bboxes_.clear();

    build_adjacency();

    if (config.enable_sizing_refinement && density_fn_) {
        LOGT_INFO(LogGeometry, "Running density refinement for circular mesh...");
        refine_to_density();

        DelaunayTriangulationResult refined = build_result_from_state();
        compute_statistics(refined);
        remove_dangling_vertices(refined);
        return refined;
    }

    return R;
}

int DelaunayTriangulator::find_blocking_vertex_on_segment(int a, int b) const {
    const glm::dvec2 A = points[a].p;
    const glm::dvec2 B = points[b].p;
    const glm::dvec2 AB = B - A;
    const double ab2 = AB.x*AB.x + AB.y*AB.y;
    if (ab2 < 1e-30) return -1;

    const double eps = std::max(1e-6, config.epsilon * 1e7);

    int best = -1;
    double best_t = 1e300;

    for (int i = 0; i < (int)points.size(); ++i) {
        if (i == a || i == b) continue;
        const glm::dvec2 P = points[i].p;

        if (!point_on_segment_eps(P, A, B, eps)) continue;

        const glm::dvec2 AP = P - A;
        const double t = (AP.x*AB.x + AP.y*AB.y) / ab2; // projection parameter
        if (t > 1e-9 && t < 1.0 - 1e-9 && t < best_t) {
            best_t = t;
            best = i;
        }
    }
    return best;
}

}

