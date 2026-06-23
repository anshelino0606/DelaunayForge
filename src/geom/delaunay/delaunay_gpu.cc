#include "delaunay_gpu.h"
#include <glm/glm.hpp>
#include <cstdio>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <algorithm>
#include "math/density_functions.h"
#include "geom/geometry_2d.h"
#include <iostream>
#include "log_categories.h"

namespace fem {

namespace {
    static bool point_in_poly(const std::vector<Point2D>& poly,
                              const glm::dvec2& q)
    {
        const size_t n = poly.size();
        if (n < 3) return false;

        bool inside = false;
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            glm::dvec2 pi = poly[i].p;
            glm::dvec2 pj = poly[j].p;
            const bool cond = ((pi.y > q.y) != (pj.y > q.y));
            if (!cond) continue;

            double x_intersect =
                pi.x + (pj.x - pi.x) * (q.y - pi.y) / (pj.y - pi.y + 1e-16);

            if (q.x < x_intersect)
                inside = !inside;
        }
        return inside;
    }

    static bool pointInPolyIdx(const std::vector<Point2D>& P,
                               const std::vector<int>& idx,
                               double x, double y)
    {
        if (idx.size() < 3) return false;
        bool inside = false;
        int n = (int)idx.size();
        for (int i = 0, j = n - 1; i < n; j = i++) {
            const auto& a = P[idx[i]];
            const auto& b = P[idx[j]];
            bool inter = ((a.y() > y) != (b.y() > y)) &&
                         (x < (b.x() - a.x()) * (y - a.y()) / ((b.y() - a.y()) + 1e-300) + a.x());
            if (inter) inside = !inside;
        }
        return inside;
    }
}


GPUDelaunayTriangulator::GPUDelaunayTriangulator()
    : mode_(Mode::FULL_GPU)
    , initialized_(false)
{
    stats_.reset();
}

GPUDelaunayTriangulator::~GPUDelaunayTriangulator() {
    shutdown();
}

bool GPUDelaunayTriangulator::init() {
    if (initialized_) return true;
    return true;
}

void GPUDelaunayTriangulator::shutdown() {
    if (!initialized_) return;
    initialized_ = false;
}

DelaunayTriangulationResult GPUDelaunayTriangulator::triangulate(
    const std::vector<Point2D>& points,
    const DelaunayTriangulationConfig& config)
{
    auto start = std::chrono::high_resolution_clock::now();
    stats_.reset();
    
    DelaunayTriangulationResult result;
    
    if (!initialized_) {
        DelaunayTriangulator cpu_tri(config);
        return cpu_tri.triangulate(points);
    }
    
    if (mode_ == Mode::FULL_GPU) {
        result = triangulate_full_gpu(points, config);
    } else {
        result = triangulate_hybrid(points, config);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    stats_.total_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    return result;
}

DelaunayTriangulationResult GPUDelaunayTriangulator::triangulate_with_boundary(
    const std::vector<Point2D>& input_points,
    const std::vector<Point2D>& boundary_poly_ccw,
    const DelaunayTriangulationConfig& config)
{
    DelaunayTriangulator cpu_tri(config);
    return cpu_tri.triangulate_with_boundary(input_points, boundary_poly_ccw);
}

std::vector<int> GPUDelaunayTriangulator::find_bad_triangles_gpu(
    const std::vector<Point2D>& points,
    const std::vector<Tri>& triangles,
    const Point2D& test_point,
    double epsilon)
{
    return {};
}


std::vector<Edge> GPUDelaunayTriangulator::extract_cavity_boundary_cpu(
    const std::vector<Tri>& triangles,
    const std::vector<int>& bad_triangle_ids)
{
    auto cpu_start = std::chrono::high_resolution_clock::now();
    
    std::unordered_map<long long, int> edge_count;
    
    for (int tri_idx : bad_triangle_ids) {
        const Tri& tri = triangles[tri_idx];
        edge_count[pack_edge(tri.v[0], tri.v[1])]++;
        edge_count[pack_edge(tri.v[1], tri.v[2])]++;
        edge_count[pack_edge(tri.v[2], tri.v[0])]++;
    }
    
    std::vector<Edge> boundary;
    for (const auto& pair : edge_count) {
        if (pair.second == 1) {
            int a = static_cast<int>(pair.first >> 32);
            int b = static_cast<int>(pair.first & 0xffffffffu);
            boundary.emplace_back(a, b);
        }
    }
    
    auto cpu_end = std::chrono::high_resolution_clock::now();
    stats_.cpu_time_us += std::chrono::duration_cast<std::chrono::microseconds>(cpu_end - cpu_start).count();
    
    return boundary;
}

DelaunayTriangulationResult GPUDelaunayTriangulator::triangulate_hybrid(
    const std::vector<Point2D>& input_points,
    const DelaunayTriangulationConfig& config)
{
    DelaunayTriangulationResult result;
    
    if (input_points.size() < 3) {
        result.points = input_points;
        return result;
    }
    
    DelaunayTriangulator cpu_helper(config);
    
    std::vector<Point2D> points = input_points;
    std::vector<Tri> triangles;
    
    for (size_t i = 0; i < points.size(); ++i) {
        points[i].id = static_cast<int>(i);
    }
    
    double xmin = points[0].x(), xmax = points[0].x();
    double ymin = points[0].y(), ymax = points[0].y();
    
    for (const auto& p : points) {
        xmin = std::min(xmin, p.x());
        xmax = std::max(xmax, p.x());
        ymin = std::min(ymin, p.y());
        ymax = std::max(ymax, p.y());
    }
    
    double dx = xmax - xmin;
    double dy = ymax - ymin;
    double dmax = std::max(dx, dy) * 10.0;
    double d2 = dx*dx + dy*dy;

    // e.g. 1e-6 of squared diag
    double eps_geom = 1e-6 * d2;
    double epsilon  = (config.epsilon > 0.0) ? config.epsilon : eps_geom;

    
    double cx = (xmin + xmax) * 0.5;
    double cy = (ymin + ymax) * 0.5;
    
    points.emplace_back(cx, cy + 2*dmax, static_cast<int>(points.size()));
    points.emplace_back(cx - 1.732*dmax, cy - dmax, static_cast<int>(points.size()));
    points.emplace_back(cx + 1.732*dmax, cy - dmax, static_cast<int>(points.size()));
    
    int s0 = static_cast<int>(points.size()) - 3;
    int s1 = static_cast<int>(points.size()) - 2;
    int s2 = static_cast<int>(points.size()) - 1;
    
    triangles.emplace_back(s0, s1, s2, 0);
    
    int original_count = static_cast<int>(input_points.size());
    
    for (int i = 0; i < original_count; ++i) {
        const Point2D& new_point = points[i];
        
        std::vector<int> bad_triangles = find_bad_triangles_gpu(
            points, triangles, new_point, epsilon
        );
        
        if (bad_triangles.empty()) continue;
        
        std::vector<Edge> cavity_boundary = extract_cavity_boundary_cpu(
            triangles, bad_triangles
        );
        
        for (int tri_id : bad_triangles) {
            triangles[tri_id].valid = false;
        }
        
        for (const Edge& edge : cavity_boundary) {
            int tri_id = static_cast<int>(triangles.size());
            Tri new_tri(edge.a, edge.b, i, tri_id);
            
            // Ensure CCW
            glm::dvec2 a = points[new_tri.v[0]].p;
            glm::dvec2 b = points[new_tri.v[1]].p;
            glm::dvec2 c = points[new_tri.v[2]].p;
            
            if (orient_sign(a, b, c) < 0) {
                std::swap(new_tri.v[0], new_tri.v[1]);
            }
            
            triangles.push_back(new_tri);
        }
    }
    
    std::vector<Tri> valid_triangles;
    valid_triangles.reserve(triangles.size());
    
    for (const Tri& tri : triangles) {
        if (!tri.valid) continue;
        
        if (tri.v[0] == s0 || tri.v[0] == s1 || tri.v[0] == s2 ||
            tri.v[1] == s0 || tri.v[1] == s1 || tri.v[1] == s2 ||
            tri.v[2] == s0 || tri.v[2] == s1 || tri.v[2] == s2) {
            continue;
        }
        
        bool valid = true;
        for (int j = 0; j < 3; ++j) {
            if (tri.v[j] < 0 || tri.v[j] >= original_count) {
                valid = false;
                break;
            }
        }
        
        if (valid) {
            Tri new_tri = tri;
            new_tri.id = static_cast<int>(valid_triangles.size());
            valid_triangles.push_back(new_tri);
        }
    }
    
    triangles = std::move(valid_triangles);
    points.resize(original_count);
    result.points = points;
    result.triangles = triangles;
    
    cpu_helper.points = points;
    cpu_helper.triangles = triangles;
    cpu_helper.build_adjacency();
    
    result.tri2vert.resize(triangles.size());
    result.tri_neighbors.resize(triangles.size());
    for (size_t i = 0; i < triangles.size(); ++i) {
        const Tri& t = triangles[i];
        result.tri2vert[i] = {t.v[0], t.v[1], t.v[2]};
        result.tri_neighbors[i] = {t.neighbors[0], t.neighbors[1], t.neighbors[2]};
    }
    
    result.vert2tri.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        result.vert2tri[i] = points[i].incident_triangles;
    }
    
    result.edges = cpu_helper.edges_cache_;
    result.tri2edge = cpu_helper.tri2edge_cache_;
    
    cpu_helper.compute_statistics(result);
    
    return result;
}

DelaunayTriangulationResult GPUDelaunayTriangulator::triangulate_full_gpu(
    const std::vector<Point2D>& input_points,
    const DelaunayTriangulationConfig& config)
{
    return {};
}

DelaunayTriangulationResult GPUDelaunayTriangulator::refine_to_density_gpu(
    DelaunayTriangulationResult& initial_result,
    std::shared_ptr<DensityFunction> density_fn,
    const DelaunayTriangulationConfig& config)
{
    return {};
}


DelaunayTriangulationResult GPUDelaunayTriangulator::triangulate_with_boundary_gpu(
    const std::vector<Point2D>& input_points,
    const std::vector<Point2D>& boundary_poly_ccw,
    const DelaunayTriangulationConfig& config)
{
    std::vector<std::vector<Point2D>> loops(1);
    loops[0] = boundary_poly_ccw;
    return triangulate_with_boundaries_gpu(input_points, loops, config);
}

DelaunayTriangulationResult GPUDelaunayTriangulator::triangulate_with_boundaries_gpu(
    const std::vector<Point2D>& input_points,
    const std::vector<std::vector<Point2D>>& loops_ccw_outer_cw_holes,
    const DelaunayTriangulationConfig& config)
{
    using Result = DelaunayTriangulationResult;
    Result R;

    if (loops_ccw_outer_cw_holes.empty()) {
        return triangulate_full_gpu(input_points, config);
    }

    std::vector<Point2D> pts = input_points;

    auto find_or_add = [&](const Point2D& q) {
        for (size_t i = 0; i < pts.size(); ++i) {
            if (std::abs(pts[i].x() - q.x()) < 1e-12 &&
                std::abs(pts[i].y() - q.y()) < 1e-12)
                return (int)i;
        }
        pts.emplace_back(q.x(), q.y(), (int)pts.size());
        return (int)pts.size() - 1;
    };

    std::vector<std::vector<int>> loop_idx(loops_ccw_outer_cw_holes.size());

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

    for (size_t L = 0; L < loops_ccw_outer_cw_holes.size(); ++L) {
        std::vector<Point2D> loop = loops_ccw_outer_cw_holes[L];
        double A = signed_area(loop);

        if (L == 0) {
            if (A < 0.0) std::reverse(loop.begin(), loop.end()); // outer CCW
        } else {
            if (A > 0.0) std::reverse(loop.begin(), loop.end()); // holes CW
        }

        loop_idx[L].reserve(loop.size());
        for (const auto& p : loop) {
            int id = find_or_add(p);
            loop_idx[L].push_back(id);
            pts[id].on_boundary = true;
        }
    }

    R = triangulate_full_gpu(pts, config);
    
    for (const auto& L : loop_idx) {
        for (int id : L) {
            if (id >= 0 && id < (int)R.points.size())
                R.points[id].on_boundary = true;
        }
    }

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

    if (loop_idx[0].size() >= 3) {
        auto inside_outer = [&](double x, double y) {
            return pointInPolyIdx(R.points, loop_idx[0], x, y);
        };
        auto inside_loop = [&](const std::vector<int>& L, double x, double y) {
            return pointInPolyIdx(R.points, L, x, y);
        };

        std::vector<Tri> kept;
        kept.reserve(R.triangles.size());

        for (const auto& t : R.triangles) {
            const auto& A = R.points[t.v[0]];
            const auto& B = R.points[t.v[1]];
            const auto& C = R.points[t.v[2]];

            glm::dvec2 c = Geometry2D::tri_centroid(A, B, C);
            if (!inside_outer(c.x, c.y)) continue;

            bool in_hole = false;
            for (size_t h = 1; h < loop_idx.size(); ++h) {
                if (loop_idx[h].size() < 3) continue;
                if (inside_loop(loop_idx[h], c.x, c.y)) {
                    in_hole = true;
                    break;
                }
            }

            if (!in_hole) kept.push_back(t);
        }

        R.triangles.swap(kept);

        compute_result_connectivity_and_stats(R, config);
    }

    return R;
}

bool GPUDelaunayTriangulator::clip_to_loops_gpu(
    DelaunayTriangulationResult& io,
    const std::vector<std::vector<Point2D>>& loops_ccw)
{
    return true;
}

GPUDelaunayTriangulator::ConflictGraph GPUDelaunayTriangulator::build_conflict_graph_gpu(
    const std::vector<Point2D>& points,
    const std::vector<Tri>& triangles)
{
    // TODO: Build conflict graph on GPU
    // For each (point, triangle) pair, check if point conflicts with triangle
    // A point conflicts with a triangle if it lies inside its circumcircle
    
    ConflictGraph graph;
    graph.point_to_triangles.resize(points.size());
    graph.triangle_to_points.resize(triangles.size());
    
    // Placeholder: would dispatch compute shader
    // This is essentially a 2D incircle test matrix
    
    return graph;
}

std::vector<int> GPUDelaunayTriangulator::find_independent_points(
    const ConflictGraph& graph,
    const std::vector<bool>& already_inserted)
{
    // TODO: Find maximal independent set
    // Points are independent if their conflict zones don't overlap
    // This can be done with parallel graph coloring
    
    std::vector<int> independent;
    
    // Greedy approach (can be parallelized):
    // For each uninserted point, check if it conflicts with any already-selected points
    
    return independent;
}

std::vector<Edge> GPUDelaunayTriangulator::extract_cavity_boundary_gpu(
    const std::vector<Tri>& triangles,
    const std::vector<int>& bad_triangle_ids)
{
    // TODO: GPU-accelerated cavity extraction
    // Use atomic operations to count edges
    // Edges appearing exactly once form the cavity boundary
    
    std::vector<Edge> boundary;
    
    // if (!bgfx::isValid(cavity_program_)) {
    //     // Fall back to CPU version
    //     return extract_cavity_boundary_cpu(triangles, bad_triangle_ids);
    // }
    
    // Placeholder: would dispatch cs_cavity.sc
    
    return boundary;
}

std::unique_ptr<GPUDelaunayTriangulator> create_delaunay_triangulator(
    GPUDelaunayTriangulator::Mode mode)
{
    auto triangulator = std::make_unique<GPUDelaunayTriangulator>();
    
    if (triangulator->init()) {
        triangulator->set_mode(mode);
        return triangulator;
    }
    
    return nullptr;
}

void GPUDelaunayTriangulator::compute_result_connectivity_and_stats(
    DelaunayTriangulationResult& R,
    const DelaunayTriangulationConfig& config)
{
    DelaunayTriangulator cpu(config);
    cpu.points = R.points;
    cpu.triangles = R.triangles;
    cpu.build_adjacency();

    R.tri2vert.resize(R.triangles.size());
    R.tri_neighbors.resize(R.triangles.size());
    for (size_t i = 0; i < R.triangles.size(); ++i) {
        const auto& t = R.triangles[i];
        R.tri2vert[i]     = { t.v[0], t.v[1], t.v[2] };
        R.tri_neighbors[i]= { t.neighbors[0], t.neighbors[1], t.neighbors[2] };
    }

    R.vert2tri.resize(R.points.size());
    for (size_t i = 0; i < R.points.size(); ++i) {
        R.vert2tri[i] = cpu.points[i].incident_triangles;
    }

    R.edges    = cpu.edges_cache_;
    R.tri2edge = cpu.tri2edge_cache_;
    cpu.compute_statistics(R);
}

}
