#pragma once

#ifdef USE_BGFX

#include "delaunay2d.h"
#include "rhi/storage_buffer.h"
#include "rhi/compute_program.h"
#include "rhi/ping_pong_storage_buffers.h"
#include "math/density_functions.h"
#include <vector>
#include <memory>

namespace fem {

class GPUDelaunayTriangulator {
public:
    enum class Mode {
        HYBRID,        // CPU main loop, GPU incircle tests
        FULL_GPU       // Everything on GPU (NOT DONE YET IM A STOOBID FUCK)
    };
    
    GPUDelaunayTriangulator();
    ~GPUDelaunayTriangulator();
    
    bool init(const char* shader_dir);
    void shutdown();
    
    /**
     * Main triangulation interface
     * Automatically selects best implementation based on mode
     */
    DelaunayTriangulationResult triangulate(
        const std::vector<Point2D>& points,
        const DelaunayTriangulationConfig& config = DelaunayTriangulationConfig{}
    );
    
    /**
     * Triangulation with boundary
     */
    DelaunayTriangulationResult triangulate_with_boundary(
        const std::vector<Point2D>& input_points,
        const std::vector<Point2D>& boundary_poly_ccw,
        const DelaunayTriangulationConfig& config = DelaunayTriangulationConfig{}
    );
    
    void set_mode(Mode mode) { mode_ = mode; }
    Mode get_mode() const { return mode_; }
    
    struct Stats {
        uint64_t total_time_us;
        uint64_t gpu_time_us;
        uint64_t cpu_time_us;
        uint32_t num_incircle_tests;
        uint32_t num_gpu_dispatches;
        
        void reset() {
            total_time_us = 0;
            gpu_time_us = 0;
            cpu_time_us = 0;
            num_incircle_tests = 0;
            num_gpu_dispatches = 0;
        }
    };
    
    const Stats& get_stats() const { return stats_; }

    DelaunayTriangulationResult refine_to_density_gpu(
        DelaunayTriangulationResult& initial_result,
        std::shared_ptr<DensityFunction> density_fn,
        const DelaunayTriangulationConfig& config
    );

    
    DelaunayTriangulationResult triangulate_with_boundary_gpu(
    const std::vector<Point2D>& input_points,
    const std::vector<Point2D>& boundary_poly_ccw,
    const DelaunayTriangulationConfig& config);

    DelaunayTriangulationResult triangulate_with_boundaries_gpu(
        const std::vector<Point2D>& input_points,
        const std::vector<std::vector<Point2D>>& loops_ccw,
        const DelaunayTriangulationConfig& config);
    
            
    DelaunayTriangulationResult triangulate_full_gpu(
        const std::vector<Point2D>& points,
        const DelaunayTriangulationConfig& config
    );
    
    void set_density_function(std::shared_ptr<DensityFunction> f) {
        density_fn_ = std::move(f);
    }


private:
    Mode mode_;
    bool initialized_;
    Stats stats_;

    // Compute programs
    fem::ComputeProgram incircle_program_;
    fem::ComputeProgram cavity_program_;
    fem::ComputeProgram point_location_program_;  // Future: parallel point location
    fem::ComputeProgram batch_insert_program_;     // Future: batched insertion
    
    bgfx::UniformHandle u_params_;
    
    fem::StorageBuffer<glm::vec4> points_buffer_;
    fem::StorageBuffer<glm::uvec4> triangles_buffer_;
    fem::StorageBuffer<glm::vec4> test_point_buffer_;
    // fem::StorageBuffer edges_buffer_;    // ??
    
    bgfx::DynamicIndexBufferHandle cavity_edges_buffer_;
    
    uint32_t points_capacity_;
    uint32_t triangles_capacity_;
    uint32_t edges_capacity_;

    fem::ComputeProgram cs_clip_tris_;
    fem::ComputeProgram cs_update_neighbors_;
    fem::ComputeProgram cs_insert_points_;
    fem::ComputeProgram cs_update_topology_;

    std::shared_ptr<DensityFunction> density_fn_;

    fem::StorageBuffer<glm::ivec4> neighbors_buffer_;   // ivec4 per tri
    fem::StorageBuffer<glm::ivec4> seedtri_buffer_;     // int per point
    fem::StorageBuffer<glm::uvec4> pstatus_buffer_;     // uint per point (0,1,2)
    fem::StorageBuffer<glm::uvec4> edgearena_buffer_;   // uvec2 per boundary edge

    fem::StorageBuffer<glm::vec4> loops_buffer_; // vec4 (x,y,0,0)
    fem::StorageBuffer<glm::ivec4> loops_meta_buffer_; // ivec4(start,count,is_outer,0)
    uint32_t loops_capacity_;
    uint32_t loops_meta_capacity_;

    fem::PingPongStorageBuffers<uint32_t> counters_buf_;      // RWBuffer<uint> @ binding 9

    fem::ComputeProgram cs_refine_sizing_;
    fem::StorageBuffer<glm::vec4> steiner_candidates_buffer_;
    uint32_t steiner_capacity_;

    fem::StorageBuffer<glm::ivec4> owner_buffer_;
    uint32_t owner_capacity_ = 0;

    // capacity
    uint32_t neighbors_capacity_;
    uint32_t seedtri_capacity_;
    uint32_t pstatus_capacity_;
    uint32_t edgearena_capacity_;

    bool are_gpu_triangulate_shaders_valid() const;
    
    std::vector<uint32_t> read_counters_from_gpu();
    void clear_counters(uint32_t c0 = 0, uint32_t c1 = 0, uint32_t c2 = 0, uint32_t c3 = 0);

    /**
     * Hybrid Bowyer-Watson: CPU main loop with GPU acceleration
     * This is the current working implementation
     */
    DelaunayTriangulationResult triangulate_hybrid(
        const std::vector<Point2D>& points,
        const DelaunayTriangulationConfig& config
    );


    
    /**
     * GPU-accelerated bad triangle finding
     * Returns indices of triangles that fail incircle test
     */
    std::vector<int> find_bad_triangles_gpu(
        const std::vector<Point2D>& points,
        const std::vector<Tri>& triangles,
        const Point2D& test_point,
        double epsilon
    );
    
    /**
     * Extract cavity boundary from bad triangles
     * CPU implementation - can be moved to GPU later
     */
    std::vector<Edge> extract_cavity_boundary_cpu(
        const std::vector<Tri>& triangles,
        const std::vector<int>& bad_triangle_ids
    );


    /**
     * GPU-accelerated cavity boundary extraction
     * Future optimization
     */
    std::vector<Edge> extract_cavity_boundary_gpu(
        const std::vector<Tri>& triangles,
        const std::vector<int>& bad_triangle_ids
    );

    
    /**
     * Parallel point location
     * For each point, find which triangle it falls into
     */
    std::vector<int> locate_points_gpu(
        const std::vector<Point2D>& points,
        const std::vector<Tri>& triangles
    );
    
    void ensure_loops_capacity(uint32_t loopVerts, uint32_t numLoops);

    /**
     * Build conflict graph
     * Identifies which points conflict with which triangles
     */
    struct ConflictGraph {
        std::vector<std::vector<int>> point_to_triangles;
        std::vector<std::vector<int>> triangle_to_points;
    };
    
    ConflictGraph build_conflict_graph_gpu(
        const std::vector<Point2D>& points,
        const std::vector<Tri>& triangles
    );
    
    std::vector<int> find_independent_points(
        const ConflictGraph& graph,
        const std::vector<bool>& already_inserted
    );
    
    bgfx::ShaderHandle loadComputeShader(const char* path);
    
    void upload_mesh_state(
        const std::vector<Point2D>& points,
        const std::vector<Tri>& triangles
    );
    
    void ensure_capacity(uint32_t num_points, uint32_t num_triangles, uint32_t num_boundary_edges = 0);
    
    inline long long pack_edge(int a, int b) const {
        if (a > b) std::swap(a, b);
        return ((long long)a << 32) | (unsigned)b;
    }
    
    uint64_t get_time_us() const;

    void compute_result_connectivity_and_stats(
        DelaunayTriangulationResult& R,
        const DelaunayTriangulationConfig& config);

    bool clip_to_loops_gpu(DelaunayTriangulationResult& io,
                       const std::vector<std::vector<Point2D>>& loops_ccw);
};


std::unique_ptr<GPUDelaunayTriangulator> create_delaunay_triangulator(
    const char* shader_dir = nullptr,
    GPUDelaunayTriangulator::Mode mode = GPUDelaunayTriangulator::Mode::HYBRID
);

}

#endif // USE_BGFX