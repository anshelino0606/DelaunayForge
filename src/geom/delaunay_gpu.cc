#ifdef USE_BGFX

#include "delaunay_gpu.h"
#include <glm/glm.hpp>
// #include "geom_utils.h"
#include <cstdio>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <algorithm>
#include "math/density_functions.h"
#include <iostream>
#include "log_categories.h"

namespace fem {

// counters[0] = edge_count   (written by cs_boundary)
// counters[1] = tri_count    (written ONLY by cs_rebuild)
// counters[2] = progress     (set by any pass if it made progress this iter)
// counters[3] = out_count    (free / misc; preserved across resets)
struct Counters { uint32_t edge_count, tri_count, progress, out_count; };

namespace {
    static constexpr uint8_t BIND_POINTS      = 0; // points SSBO
    static constexpr uint8_t BIND_TRIS        = 1; // triangles SSBO
    static constexpr uint8_t BIND_NEIGHBORS   = 2; // neighbors SSBO
    static constexpr uint8_t BIND_SEEDTRI     = 3; // seed triangle per point
    static constexpr uint8_t BIND_PSTATUS     = 4; // point status (0/1/2)
    static constexpr uint8_t BIND_BOUNDARY    = 5; // boundary polyline SSBO
    static constexpr uint8_t BIND_EDGEARENA   = 6; // edge arena
    static constexpr uint8_t BIND_OWNER       = 8; // owners
    static constexpr uint8_t BIND_COUNTERS    = 9; // RWBuffer<uint> counters[4..]
    static constexpr uint8_t BIND_PREV_COUNTERS = 10;

    static constexpr uint8_t IMG_COUNTERS     = 0; // R32U scratch/readback (dump_uints, incircle flags)
    static constexpr uint8_t IMG_TRI_OUT      = 1; // RGBA32F triangle dump (cs_dump_tris)

    static constexpr uint8_t IMG_VEC4_OUT     = 2; // RGBA32F generic vec4 dump (cs_dump_vec4)

    static constexpr uint32_t MAX_ITERS       = 16;

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
    , points_capacity_(0)
    , triangles_capacity_(0)
    , edges_capacity_(0)
    , owner_capacity_(0)
    , neighbors_capacity_(0)
    , seedtri_capacity_(0)
    , pstatus_capacity_(0)
    , edgearena_capacity_(0)
    , loops_capacity_(0)
    , loops_meta_capacity_(0)
{
    stats_.reset();
}

GPUDelaunayTriangulator::~GPUDelaunayTriangulator() {
    shutdown();
}

bool GPUDelaunayTriangulator::init(const char* shader_dir) {
    if (initialized_) return true;

    incircle_program_.init("cs_incircle.bin");
    cavity_program_.init("cs_cavity.bin");
    cs_refine_sizing_.init("cs_refine_sizing.bin");
    cs_clip_tris_.init("cs_clip_triangles.bin");
    cs_update_neighbors_.init("cs_update_neighbors.bin");
    cs_insert_points_.init("cs_insert_points.bin");
    cs_update_topology_.init("cs_update_topology.bin");

    if (!cs_refine_sizing_.is_valid()) {
        LOGT_ERROR(LogGeometry, "Could not load cs_refine_sizing.bin (not built or not found)");
    }

    if (!are_gpu_triangulate_shaders_valid()) {
        LOGT_ERROR(LogGeometry, "Failed to load one or more shaders.");
        return false;
    }
    
    u_params_ = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);

    ensure_capacity(10000, 50000, 10000);

    initialized_ = true;
    stats_.reset();
    return true;
}

void GPUDelaunayTriangulator::shutdown() {
    if (!initialized_) return;

    bgfx::destroy(u_params_);

    incircle_program_.destroy();
    cavity_program_.destroy();
    point_location_program_.destroy();
    batch_insert_program_.destroy();
    cs_update_neighbors_.destroy();
    points_buffer_.destroy();
    triangles_buffer_.destroy();
    neighbors_buffer_.destroy();
    seedtri_buffer_.destroy();
    pstatus_buffer_.destroy();
    edgearena_buffer_.destroy();
    owner_buffer_.destroy();
    test_point_buffer_.destroy();
    cs_refine_sizing_.destroy();
    steiner_candidates_buffer_.destroy();
    cs_refine_sizing_.destroy();
    steiner_candidates_buffer_.destroy();
    cs_clip_tris_.destroy();
    loops_buffer_.destroy();
    loops_meta_buffer_.destroy();
    cs_update_topology_.destroy();
    cs_insert_points_.destroy();
    
    initialized_ = false;
}

void GPUDelaunayTriangulator::ensure_loops_capacity(uint32_t loopVerts, uint32_t numLoops)
{
    // bgfx::VertexLayout vec4_layout;
    // vec4_layout.begin().add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float).end();

    // auto grow = [&](bgfx::DynamicVertexBufferHandle& h, uint32_t& cap, uint32_t need, const char* what)
    // {
    //     if (need == 0) need = 1;
    //     if (!bgfx::isValid(h) || need > cap) {
    //         if (bgfx::isValid(h)) bgfx::destroy(h);
    //         cap = std::max(need, cap);
    //         cap = cap + cap / 2;
    //         h = bgfx::createDynamicVertexBuffer(cap, vec4_layout, BGFX_BUFFER_COMPUTE_READ);
    //     }
    // };

    // grow(loops_buffer_,      loops_capacity_,      loopVerts, "loops_buffer_");
    // grow(loops_meta_buffer_, loops_meta_capacity_, numLoops,  "loops_meta_buffer_");
}


void GPUDelaunayTriangulator::ensure_capacity(uint32_t num_points,
                                              uint32_t num_triangles,
                                              uint32_t num_boundary_edges)
{
    auto grow = [&]<typename T>(fem::StorageBuffer<T>& storage_buffer, uint32_t& capVec4,
                    uint32_t needVec4, const char* what)
    {
        if (needVec4 == 0) needVec4 = 1;                    // never 0-sized

        if (!storage_buffer.is_valid() || needVec4 > capVec4) {
            capVec4 = std::max(needVec4, capVec4);
            capVec4 = capVec4 + capVec4 / 2;                // +50% headroom

            storage_buffer.grow(capVec4);

            LOGT_DEBUG(LogGeometry, "(%s) new capacity %u vec4s (%u bytes)",
                         what, capVec4, capVec4*16u);
        }
    };

    // All compute SSBOs are vec4-arrays.
    uint32_t need_points_vec4    = std::max(1u, num_points);
    uint32_t need_tris_vec4      = std::max(1u, num_triangles);
    uint32_t need_neighbors_vec4 = std::max(1u, num_triangles);
    uint32_t need_seedtri_vec4   = std::max(1u, num_points);
    uint32_t need_pstatus_vec4   = std::max(1u, num_points);
    uint32_t need_owner_vec4     = std::max(1u, num_triangles);

    if (!test_point_buffer_.is_valid()) {
        test_point_buffer_.init(1);
    }

    grow(points_buffer_,     points_capacity_,    need_points_vec4,    "points_buffer_");
    grow(triangles_buffer_,  triangles_capacity_, need_tris_vec4,      "triangles_buffer_");
    grow(neighbors_buffer_,  neighbors_capacity_, need_neighbors_vec4, "neighbors_buffer_");
    grow(seedtri_buffer_,    seedtri_capacity_,   need_seedtri_vec4,   "seedtri_buffer_");
    grow(pstatus_buffer_,    pstatus_capacity_,   need_pstatus_vec4,   "pstatus_buffer_");
    grow(owner_buffer_,      owner_capacity_,     need_owner_vec4,     "owner_buffer_");

    uint32_t edge_arena_needed_vec4 = std::max(4096u, num_boundary_edges * 2);
    grow(edgearena_buffer_, edgearena_capacity_, edge_arena_needed_vec4, "edgearena_buffer_");

    // Room for many candidates; safe upper bound ~3 per tri
    uint32_t steiner_needed_vec4 = std::max(1u, 3u * std::max(1u, num_triangles));
    grow(steiner_candidates_buffer_, steiner_capacity_, steiner_needed_vec4, "steiner_candidates_buffer_");


    if (!counters_buf_.is_valid()) {
        counters_buf_.init(16);

        if (!counters_buf_.is_valid()) {
            LOGT_ERROR(LogGeometry, "Failed to create counters_buf_.");
        }
    }
}

void GPUDelaunayTriangulator::upload_mesh_state(
    const std::vector<Point2D>& points,
    const std::vector<Tri>& triangles)
{
    // ensure_capacity((uint32_t)points.size(), (uint32_t)triangles.size());
    
    // std::vector<glm::vec4> point_data(points.size());
    // for (size_t i = 0; i < points.size(); ++i) {
    //     point_data[i] = glm::vec4(points[i].x, points[i].y, 0.0f, 
    //                                points[i].on_boundary ? 1.0f : 0.0f);
    // }
    
    // const bgfx::Memory* pmem = bgfx::copy(point_data.data(), 
    //     (uint32_t)(point_data.size() * sizeof(glm::vec4)));
    // bgfx::update(points_buffer_, 0, pmem);
    
    // std::vector<glm::uvec4> tri_data(triangles.size());
    // for (size_t i = 0; i < triangles.size(); ++i) {
    //     tri_data[i] = glm::uvec4(
    //         triangles[i].v[0],
    //         triangles[i].v[1],
    //         triangles[i].v[2],
    //         triangles[i].valid ? 1u : 0u
    //     );
    // }
    
    // const bgfx::Memory* tmem = bgfx::copy(tri_data.data(),
    //     (uint32_t)(tri_data.size() * sizeof(glm::uvec4)));
    // bgfx::update(triangles_buffer_, 0, tmem);
}

bool GPUDelaunayTriangulator::are_gpu_triangulate_shaders_valid() const {
    return  cs_update_topology_.is_valid() && cs_insert_points_.is_valid();
}

std::vector<uint32_t> GPUDelaunayTriangulator::read_counters_from_gpu() {
    std::vector<uint32_t> counters;
    counters_buf_.read_current(counters, 4);

    counters.resize(4);

    return counters;
}

void GPUDelaunayTriangulator::clear_counters(uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3) {
    std::vector<uint32_t> data = {c0, c1, c2, c3};
    counters_buf_.update_all(data);
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
    
    if (mode_ == Mode::FULL_GPU && are_gpu_triangulate_shaders_valid()) {
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
    // std::vector<int> bad_triangles;
    // if (!initialized_ || triangles.empty()) return bad_triangles;

    // auto gpu_start = std::chrono::high_resolution_clock::now();

    // const uint32_t num_tris = (uint32_t)triangles.size();
    // ensure_capacity((uint32_t)points.size(), (uint32_t)triangles.size(), 0);

    // upload_mesh_state(points, triangles);

    // glm::vec4 test_pt(test_point.x, test_point.y, 0.0f, 0.0f);
    // const bgfx::Memory* test_mem = bgfx::copy(&test_pt, sizeof(glm::vec4));
    // bgfx::update(test_point_buffer_, 0, test_mem);

    // float params[4] = { (float)num_tris, (float)epsilon, 0.0f, 0.0f };
    // bgfx::setUniform(u_params_, params);

    // // Buffers: points, tris, test-pt
    // bgfx::setBuffer(BIND_POINTS,    points_buffer_,    bgfx::Access::Read);
    // bgfx::setBuffer(BIND_TRIS,      triangles_buffer_, bgfx::Access::Read);
    // bgfx::setBuffer(BIND_NEIGHBORS, test_point_buffer_,bgfx::Access::Read); // slot reused by shader as test-pt

    // // Image for flags/readback
    // bgfx::setImage(IMG_COUNTERS, counters_rb_tex_, 0, bgfx::Access::Write, bgfx::TextureFormat::R32U);

    // const uint32_t groups = (num_tris + 63u) / 64u;
    // bgfx::dispatch(0, incircle_program_, groups, 1, 1);
    // stats_.num_gpu_dispatches++;

    // bgfx::frame();
    // bgfx::frame();

    // std::vector<uint32_t> results(num_tris);
    // bgfx::readTexture(counters_rb_tex_, results.data());

    // bgfx::frame();
    // bgfx::frame();

    // auto gpu_end = std::chrono::high_resolution_clock::now();
    // stats_.gpu_time_us += std::chrono::duration_cast<std::chrono::microseconds>(gpu_end - gpu_start).count();

    // for (uint32_t i = 0; i < num_tris; ++i) {
    //     if (results[i] != 0u && triangles[i].valid) {
    //         bad_triangles.push_back((int)i);
    //     }
    // }
    // stats_.num_incircle_tests += num_tris;
    // return bad_triangles;

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

DelaunayTriangulationResult
GPUDelaunayTriangulator::triangulate_full_gpu(
    const std::vector<Point2D>& input_points,
    const DelaunayTriangulationConfig& config)
{
    DelaunayTriangulationResult R;

    if (!are_gpu_triangulate_shaders_valid()) {
        LOGT_ERROR(LogGeometry, "[GPU] Missing compute shaders. Falling back to CPU.");
        DelaunayTriangulator cpu(config);
        return cpu.triangulate(input_points);
    }

    if (input_points.size() < 3) {
        R.points = input_points;
        return R;
    }

    LOGT_DEBUG(LogGeometry, "[GPU] Triangulate %zu points",
                 input_points.size());

    std::vector<Point2D> points = input_points;
    for (size_t i = 0; i < points.size(); ++i)
        points[i].id = static_cast<int>(i);

    int s0, s1, s2;
    double epsilon = 0.0;
    {
        double xmin = points[0].x(), xmax = xmin;
        double ymin = points[0].y(), ymax = ymin;
        for (const auto& p : points) {
            xmin = std::min(xmin, p.x());
            xmax = std::max(xmax, p.x());
            ymin = std::min(ymin, p.y());
            ymax = std::max(ymax, p.y());
        }

        double dx   = xmax - xmin;
        double dy   = ymax - ymin;
        double dmax = std::max(dx, dy) * 10.0;
        double d2   = dx*dx + dy*dy;

        double eps_geom = 1e-6 * d2;
        epsilon         = (config.epsilon > 0.0) ? config.epsilon : eps_geom;

        double cx = 0.5 * (xmin + xmax);
        double cy = 0.5 * (ymin + ymax);

        points.emplace_back(cx,                cy + 2.0 * dmax, (int)points.size());
        points.emplace_back(cx - 1.732 * dmax, cy - dmax,       (int)points.size());
        points.emplace_back(cx + 1.732 * dmax, cy - dmax,       (int)points.size());
        s0 = (int)points.size() - 3;
        s1 = s0 + 1;
        s2 = s0 + 2;

        LOGT_DEBUG(LogGeometry, "[GPU] super triangle: %d %d %d", s0, s1, s2);
    }

    std::vector<Tri> initial_tris;
    initial_tris.emplace_back(s0, s1, s2, 0);

    const uint32_t P = (uint32_t)input_points.size();

    // O(P) in 2D
    // ensure_capacity((uint32_t)points.size(),
    //                 std::max(1u, P * 3u),  // triangles_capacity_
    //                 /*edges*/ 0u);

    // Points buffer
    {
        std::vector<glm::vec4> point_data(points_capacity_, glm::vec4(0));
        for (size_t i = 0; i < points.size(); ++i) {
            point_data[i] = glm::vec4(
                (float)points[i].x(),
                (float)points[i].y(),
                0.0f,
                points[i].on_boundary ? 1.0f : 0.0f
            );
        }
        points_buffer_.update(point_data);
    }

    {
        std::vector<glm::uvec4> tri_data(triangles_capacity_, glm::uvec4(0));
        tri_data[0] = glm::uvec4(
            (uint32_t)initial_tris[0].v[0],
            (uint32_t)initial_tris[0].v[1],
            (uint32_t)initial_tris[0].v[2],
            1u  // valid flag
        );
        triangles_buffer_.update(tri_data);
    }

    // TODO: FILL THIS VIA cs_update_topology
    {
        std::vector<glm::ivec4> neigh(neighbors_capacity_, glm::ivec4(-1, -1, -1, -1));
        neighbors_buffer_.update(neigh);
    }

    // Start with tri_count = 1 (just the super-triangle)
    clear_counters(/*edge_count*/0u, /*tri_count*/1u, /*progress*/0u, /*out_count*/0u);

    for (uint32_t pid = 0; pid < P; ++pid) {
        // Set uniforms: insert exactly 1 point with global index pid
        float U[4] = {
            (float)1,             // point_count
            (float)pid,           // so only point pid processed
            (float)epsilon,       // eps
            (float)0              
        };
        bgfx::setUniform(u_params_, U);

        points_buffer_.bind    (BIND_POINTS,   bgfx::Access::Read);
        triangles_buffer_.bind (BIND_TRIS,     bgfx::Access::ReadWrite);
        counters_buf_.bind_all(BIND_COUNTERS, BIND_PREV_COUNTERS);

        // One workgroup
        const uint32_t gx = 1u;
        cs_insert_points_.dispatch(gx);

        counters_buf_.update_previous_from_current(4);

        if (pid != P - 1) {
            counters_buf_.swap();
        }
    }

    // Read back final triangle count
    std::vector<uint32_t> final_c = read_counters_from_gpu();
    const uint32_t final_tri_count = final_c[1];
    LOGT_DEBUG(LogGeometry, "[GPU] final tri_count (raw slots) = %u", final_tri_count);

    // Optional: build neighbors + edge flips on GPU (cs_update_topology)
    {
        float U[4] = {
            (float)final_tri_count,
            config.enable_edge_flipping ? 1.0f : 0.0f,
            (float)epsilon,
            0.0f
        };
        bgfx::setUniform(u_params_, U);

        points_buffer_.bind    (BIND_POINTS,    bgfx::Access::Read);
        triangles_buffer_.bind (BIND_TRIS,      bgfx::Access::ReadWrite);
        neighbors_buffer_.bind (BIND_NEIGHBORS, bgfx::Access::Write);

        const uint32_t gx = (final_tri_count + 63u) / 64u;
        if (gx > 0u)
            cs_update_topology_.dispatch(gx);
    }

    // Read back triangles and "remove super triangle"
    R.points.clear();
    R.points = input_points;
    R.triangles.clear();

    if (final_tri_count > 0u) {
        std::vector<glm::uvec4> out;
        triangles_buffer_.read(out, final_tri_count);

        const uint32_t origN = P;

        for (uint32_t i = 0; i < final_tri_count; ++i) {
            glm::uvec4 T = out[i];

            if (T.w == 0u) continue;

            if (T.x >= origN || T.y >= origN || T.z >= origN) continue;

            Tri t((int)T.x, (int)T.y, (int)T.z, (int)R.triangles.size());
            t.valid = true;
            R.triangles.push_back(t);
        }
    }

    LOGT_DEBUG(LogGeometry, "[GPU] kept %zu valid tris (super-tri removed)",
                 R.triangles.size());

    if (config.enable_sizing_refinement && density_fn_) {
        LOGT_DEBUG(LogGeometry, "[GPU] config.refine_sizing_max_steiner = %d", 
                 config.refine_sizing_max_steiner);
        R = refine_to_density_gpu(R, density_fn_, config);
    }
    {
        const size_t n = R.points.size();
        std::vector<Tri> cleaned;
        cleaned.reserve(R.triangles.size());
        for (auto &t : R.triangles) {
            bool ok =
                t.v[0] >= 0 && (size_t)t.v[0] < n &&
                t.v[1] >= 0 && (size_t)t.v[1] < n &&
                t.v[2] >= 0 && (size_t)t.v[2] < n;

            if (!ok) {
                LOGT_DEBUG(LogGeometry,
                    "[GPU] Dropping triangle %d: (%d,%d,%d) out of [0,%zu)",
                    t.id, t.v[0], t.v[1], t.v[2], n);
                continue;
            }
            t.id = (int)cleaned.size();
            cleaned.push_back(t);
        }
        R.triangles.swap(cleaned);
    }
    compute_result_connectivity_and_stats(R, config);

    return R;
}

DelaunayTriangulationResult
GPUDelaunayTriangulator::refine_to_density_gpu(
    DelaunayTriangulationResult& initial_result,
    std::shared_ptr<DensityFunction> density_fn,
    const DelaunayTriangulationConfig& config)
{
    if (!density_fn || !config.enable_sizing_refinement ||
        config.refine_sizing_max_steiner <= 0 || !cs_refine_sizing_.is_valid())
    {
        LOGT_DEBUG(LogGeometry, "[GPU density] Skipping: enable=%d max_steiner=%d shader_valid=%d",
                     config.enable_sizing_refinement, config.refine_sizing_max_steiner,
                     cs_refine_sizing_.is_valid());
        return initial_result;
    }

    DelaunayTriangulationResult current = initial_result;
    std::vector<Point2D> current_points = current.points;

    int total_added = 0;
    const int max_added = std::max(1000, config.refine_sizing_max_steiner);
    LOGT_DEBUG(LogGeometry, "[GPU density] max_steiner budget: %d", max_added);
    const int max_passes = 10;

    // Upload density parameters once
    auto combined = std::dynamic_pointer_cast<CombinedDensity>(density_fn);
    
    // Default density values
    float boundary_h_min = 1.0f, boundary_h_max = 10.0f;
    float boundary_influence = 5.0f;
    float radial_cx = 0.0f, radial_cy = 0.0f;
    float radial_r_in = 5.0f, radial_r_out = 20.0f;
    float radial_h_min = 1.0f, radial_h_max = 10.0f;
    float global_h = 5.0f;
    bool use_boundary = false, use_radial = false;
    std::vector<glm::vec4> boundary_verts;

    if (combined) {
        for (const auto& fn : combined->get_functions()) {
            if (auto* unif = dynamic_cast<UniformDensity*>(fn.get())) {
                global_h = (float)unif->get_h();
            }
            else if (auto* bnd = dynamic_cast<BoundaryDensity*>(fn.get())) {
                use_boundary = true;
                boundary_h_min = (float)bnd->get_h_min();
                boundary_h_max = (float)bnd->get_h_max();
                boundary_influence = (float)bnd->get_influence();
                
                const auto& coords = bnd->get_boundary_coords();
                boundary_verts.reserve(coords.size());
                for (const auto& c : coords) {
                    boundary_verts.emplace_back((float)c.x, (float)c.y, 0.0f, 0.0f);
                }
            }
            else if (auto* rad = dynamic_cast<RadialDensity*>(fn.get())) {
                use_radial = true;
                auto center = rad->get_center();
                radial_cx = (float)center.x;
                radial_cy = (float)center.y;
                radial_r_in = (float)rad->get_r_inner();
                radial_r_out = (float)rad->get_r_outer();
                radial_h_min = (float)rad->get_h_min();
                radial_h_max = (float)rad->get_h_max();
            }
        }
    }

    // Create boundary buffer if needed
    fem::StorageBuffer<glm::vec4> boundary_buffer;
    if (use_boundary && !boundary_verts.empty()) {
        boundary_buffer.init(std::max(1u, (uint32_t)boundary_verts.size()));
        boundary_buffer.update(boundary_verts);
    } else if (use_boundary) {
        // Need at least 1 element
        boundary_buffer.init(1);
        std::vector<glm::vec4> dummy(1, glm::vec4(0.0f));
        boundary_buffer.update(dummy);
    }

    for (int pass = 0; pass < max_passes; ++pass) {
        if (current.triangles.empty()) {
            LOGT_DEBUG(LogGeometry, "[GPU density] pass %d: no triangles, stopping", pass);
            break;
        }

        const uint32_t tri_count = (uint32_t)current.triangles.size();
        
        LOGT_DEBUG(LogGeometry, "[GPU density] pass %d: %zu points, %u triangles",
                     pass, current_points.size(), tri_count);

        // Upload current mesh state
        // ensure_capacity((uint32_t)current_points.size(), tri_count, 0);
        
        {
            std::vector<glm::vec4> point_data(points_capacity_, glm::vec4(0));
            for (size_t i = 0; i < current_points.size(); ++i) {
                point_data[i] = glm::vec4(
                    (float)current_points[i].x(),
                    (float)current_points[i].y(),
                    0.0f,
                    current_points[i].on_boundary ? 1.0f : 0.0f
                );
            }
            points_buffer_.update(point_data);
        }

        {
            std::vector<glm::uvec4> tri_data(triangles_capacity_, glm::uvec4(0));
            for (size_t i = 0; i < current.triangles.size(); ++i) {
                const auto& t = current.triangles[i];
                tri_data[i] = glm::uvec4(
                    (uint32_t)t.v[0],
                    (uint32_t)t.v[1],
                    (uint32_t)t.v[2],
                    t.valid ? 1u : 0u
                );
            }
            triangles_buffer_.update(tri_data);
        }

        // Upload neighbors (needed by shader to avoid processing edges twice)
        {
            std::vector<glm::ivec4> neigh_data(neighbors_capacity_, glm::ivec4(-1));
            for (size_t i = 0; i < current.triangles.size(); ++i) {
                const auto& t = current.triangles[i];
                neigh_data[i] = glm::ivec4(
                    t.neighbors[0],
                    t.neighbors[1],
                    t.neighbors[2],
                    -1
                );
            }
            neighbors_buffer_.update(neigh_data);
        }

        // Clear steiner counter (slot 3)
        clear_counters(0, 0, 0, 0);

        // Set uniforms
        int budget_left = max_added - total_added;
        budget_left = std::max(0, budget_left);

        // Make sure we never ask the shader to write more than we allocated
        uint32_t max_steiner_this_pass =
            (uint32_t)std::min<int>(budget_left, (int)steiner_capacity_);

        float u_params[4] = {
            (float)tri_count,
            (float)config.density_refine_threshold,
            (float)max_steiner_this_pass,
            global_h
        };
        bgfx::setUniform(u_params_, u_params);

        float u_density0[4] = {
            boundary_h_min,
            boundary_h_max,
            boundary_influence,
            (float)boundary_verts.size()
        };
        
        float u_density1[4] = {
            radial_cx, radial_cy, radial_r_in, radial_r_out
        };
        
        float u_density2[4] = {
            radial_h_min,
            radial_h_max,
            use_boundary ? 1.0f : 0.0f,
            use_radial ? 1.0f : 0.0f
        };

        bgfx::UniformHandle u_d0 = bgfx::createUniform("u_density0", bgfx::UniformType::Vec4);
        bgfx::UniformHandle u_d1 = bgfx::createUniform("u_density1", bgfx::UniformType::Vec4);
        bgfx::UniformHandle u_d2 = bgfx::createUniform("u_density2", bgfx::UniformType::Vec4);
        
        bgfx::setUniform(u_d0, u_density0);
        bgfx::setUniform(u_d1, u_density1);
        bgfx::setUniform(u_d2, u_density2);

        // Bind buffers
        points_buffer_.bind(BIND_POINTS, bgfx::Access::Read);
        triangles_buffer_.bind(BIND_TRIS, bgfx::Access::Read);
        neighbors_buffer_.bind(BIND_NEIGHBORS, bgfx::Access::Read);
        steiner_candidates_buffer_.bind(BIND_SEEDTRI, bgfx::Access::Write);
        counters_buf_.bind_all(BIND_COUNTERS, BIND_PREV_COUNTERS);
        
        if (use_boundary && boundary_buffer.is_valid()) {
            boundary_buffer.bind(BIND_BOUNDARY, bgfx::Access::Read);
        }

        // Dispatch
        const uint32_t groups = (tri_count + 63u) / 64u;
        cs_refine_sizing_.dispatch(groups);
        
        bgfx::destroy(u_d0);
        bgfx::destroy(u_d1);
        bgfx::destroy(u_d2);

        // Read back steiner count
        std::vector<uint32_t> counters = read_counters_from_gpu();
        uint32_t steiner_count = counters[3];

        LOGT_DEBUG(LogGeometry, "[GPU density] pass %d: generated %u Steiner candidates",
                     pass, steiner_count);

        if (steiner_count == 0) {
            LOGT_DEBUG(LogGeometry, "[GPU density] pass %d: no refinement needed", pass);
            break;
        }

        // Read back candidates
        std::vector<glm::vec4> candidates;
        steiner_candidates_buffer_.read(candidates, std::min(steiner_count, (uint32_t)steiner_capacity_));

        // Deduplicate and add to points
        std::vector<glm::dvec2> new_points;
        for (uint32_t i = 0; i < std::min(steiner_count, (uint32_t)candidates.size()); ++i) {
            if (candidates[i].w > 0.5f) {  // valid flag
                new_points.emplace_back(candidates[i].x, candidates[i].y);
            }
        }

        // Sort and unique
        std::sort(new_points.begin(), new_points.end(),
                  [](const glm::dvec2& a, const glm::dvec2& b) {
                      if (std::abs(a.x - b.x) > 1e-9) return a.x < b.x;
                      return a.y < b.y;
                  });
        new_points.erase(std::unique(new_points.begin(), new_points.end(),
                                     [](const glm::dvec2& a, const glm::dvec2& b) {
                                         return std::abs(a.x - b.x) < 1e-9 &&
                                                std::abs(a.y - b.y) < 1e-9;
                                     }),
                         new_points.end());

        int can_add = std::min<int>(max_added - total_added, (int)new_points.size());
        if (can_add <= 0) {
            LOGT_DEBUG(LogGeometry, "[GPU density] hit steiner limit");
            break;
        }

        LOGT_DEBUG(LogGeometry, "[GPU density] pass %d: adding %d points", pass, can_add);

        for (int i = 0; i < can_add; ++i) {
            current_points.emplace_back(new_points[i].x, new_points[i].y, (int)current_points.size());
        }
        total_added += can_add;

        // Retriangulate with new points (NO REFINEMENT in this call)
        DelaunayTriangulationConfig cfg_no_refine = config;
        cfg_no_refine.enable_sizing_refinement = false;
        cfg_no_refine.refine_sizing_max_steiner = 0;

        current = triangulate_full_gpu(current_points, cfg_no_refine);

        if (current.triangles.empty()) {
            LOGT_DEBUG(LogGeometry, "[GPU density] WARNING: retriangulation lost all triangles!");
            break;
        }
    }

    if (boundary_buffer.is_valid()) {
        boundary_buffer.destroy();
    }

    LOGT_DEBUG(LogGeometry, "[GPU density] done: total added = %d, final tris = %zu",
                 total_added, current.triangles.size());
    return current;
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

DelaunayTriangulationResult
GPUDelaunayTriangulator::triangulate_with_boundaries_gpu(
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
            double cx = (A.x() + B.x() + C.x()) / 3.0;
            double cy = (A.y() + B.y() + C.y()) / 3.0;

            if (!inside_outer(cx, cy)) continue;

            bool in_hole = false;
            for (size_t h = 1; h < loop_idx.size(); ++h) {
                if (loop_idx[h].size() < 3) continue;
                if (inside_loop(loop_idx[h], cx, cy)) {
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
    // if (!cs_clip_tris_.is_valid()) {
    //     return false;
    // }
    // if (io.triangles.empty() || loops_ccw.empty()) return true;

    // // Flatten loop vertices + meta
    // std::vector<glm::vec4> L;
    // std::vector<glm::ivec4> M;
    // L.reserve(1024);
    // M.reserve((size_t)loops_ccw.size());

    // uint32_t start = 0;
    // for (size_t i = 0; i < loops_ccw.size(); ++i) {
    //     const auto& loop = loops_ccw[i];
    //     if (loop.empty()) continue;

    //     for (const auto& p : loop) {
    //         L.emplace_back((float)p.x, (float)p.y, 0.f, 0.f);
    //     }
    //     uint32_t count = (uint32_t)loop.size();

    //     // is_outer = 0 for first loop (outer boundary), 1 for holes
    //     int is_outer = (i == 0) ? 0 : 1;
    //     M.emplace_back((int)start, (int)count, is_outer, 0);
        
    //     start += count;
    // }

    // if (L.empty() || M.empty()) return true;

    // ensure_loops_capacity((uint32_t)L.size(), (uint32_t)M.size());
    // safeUpdate(loops_buffer_,      L.data(), (uint32_t)(L.size() * sizeof(glm::vec4)),  "loops_buffer_ upload");
    // safeUpdate(loops_meta_buffer_, M.data(), (uint32_t)(M.size()* sizeof(glm::ivec4)), "loops_meta upload");

    // // Dispatch: invalidate tris outside loops (or inside holes)
    // const uint32_t triCount = (uint32_t)io.triangles.size();
      
    // bgfx::setBuffer(0, points_buffer_,      bgfx::Access::Read);
    // bgfx::setBuffer(1, triangles_buffer_,   bgfx::Access::ReadWrite);
    // bgfx::setBuffer(2, loops_buffer_,       bgfx::Access::Read);
    // bgfx::setBuffer(3, loops_meta_buffer_,  bgfx::Access::Read);
    
    // float U[4] = { float(triCount), float(L.size()), float(M.size()), 0.f };
    // bgfx::setUniform(u_params_, U);
    // bgfx::dispatch(0, cs_clip_tris_, (triCount + 63u)/64u, 1, 1);

    // bgfx::frame();
    return true;
}



GPUDelaunayTriangulator::ConflictGraph 
GPUDelaunayTriangulator::build_conflict_graph_gpu(
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

uint64_t GPUDelaunayTriangulator::get_time_us() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
}


std::unique_ptr<GPUDelaunayTriangulator> create_delaunay_triangulator(
    const char* shader_dir,
    GPUDelaunayTriangulator::Mode mode)
{
    auto triangulator = std::make_unique<GPUDelaunayTriangulator>();
    
    if (shader_dir && triangulator->init(shader_dir)) {
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


#endif // USE_BGFX