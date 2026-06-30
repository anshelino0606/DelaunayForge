// delaunay2d.h - Enhanced Delaunay triangulator
#ifndef DELAUNAY2D_H
#define DELAUNAY2D_H

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <unordered_set>
#include <span>
#include "math/density_functions.h"
#include "delaunay_types.h"
#include "geom/tri_pool.h"

namespace fem::detail {

[[nodiscard]] constexpr bool valid_index(int v, std::size_t n) noexcept {
    return v >= 0 && static_cast<std::size_t>(v) < n;
}

}
namespace fem {

class DelaunayTriangulator {
public:
    // Statistics and validation
    void compute_statistics(DelaunayTriangulationResult& result) const;
    double compute_triangle_angle(const Tri& tri, int vertex_idx) const;


    // caches built by build_adjacency()
    std::vector<EdgeInfo>      edges_cache_;
    std::vector<glm::ivec3>     tri2edge_cache_;
    std::vector<Point2D> points;
    TriPool triangles;

    // Connectivity and adjacency
    void build_adjacency();
    void improve_mesh_quality();
private:
    struct LoopBBox {
        double xmin = 1e300;
        double xmax = -1e300;
        double ymin = 1e300;
        double ymax = -1e300;

        [[nodiscard]] bool contains(double x, double y) const noexcept {
            return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
        }
    };

    DelaunayTriangulationConfig config;

    // Scale-aware tolerances for boundary processing / constraints.
    // These are computed per triangulation-with-boundaries call from the
    // boundary bbox diagonal and then reused by constraint recovery.
    double boundary_weld_eps_ = 1e-6;
    double boundary_intersect_eps_ = 1e-12;

    std::vector<Edge> boundary_edges;
    std::unordered_set<PackedEdge, PackedEdgeHash> constrained_keys_;
    std::vector<std::pair<int,int>> constraints_;     
    std::vector<std::vector<int>> active_boundary_loops_;
    std::vector<LoopBBox> active_loop_bboxes_;

    
    std::unordered_map<PackedEdge, int, PackedEdgeHash> edge_index_cache_;

    std::vector<int> active_boundary_loop_;

    void rebuild_active_loop_bboxes_(std::span<const Point2D> pts);

    void retriangulate_in_place();
    void retriangulate_in_place_unconstrained_();
    void enforce_active_loop_constraints_if_any();

    int insert_boundary_point_on_loop_edge_(int a, int b);

    void bowyer_watson();
    std::vector<int> find_bad_triangles(const Point2D& point);
    std::vector<Edge> extract_cavity_boundary(const std::vector<int>& bad_triangles);
    void remove_triangles(const std::vector<int>& triangle_ids);
    void retriangulate_cavity(int point_idx, const std::vector<Edge>& boundary);
    
    void add_super_triangle();
    void remove_super_triangle();
    
    
    void update_triangle_neighbors();
    

    bool inside_domain(double x, double y) const;
    bool should_flip_edge(int tri1, int tri2, const Edge& edge);
    bool flip_edge(int tri1, int tri2, const Edge& edge);
    
    void mark_boundary_nodes(const std::vector<Point2D>& boundary);
    void insert_constraint_edges(const std::vector<Point2D>& boundary);
    
    void clip_to_polygon(const std::vector<Point2D>& polygon);
    bool point_in_polygon(const Point2D& point, const std::vector<Point2D>& polygon) const;

    bool edge_exists(int a, int b) const;
    Edge find_first_intersecting_edge(int a, int b) const;
    bool flip_edge_if_possible(int ea, int eb);
    bool recover_constraint(int a, int b);
    void enforce_constraints();
    void build_constraints_from_loops(const std::vector<std::vector<int>>& loops);
    void constrained_delaunay_flip_pass();

    bool is_inside_active_domain(double x, double y) const;
    

    std::shared_ptr<DensityFunction> density_fn_;
    void refine_to_density();

    [[nodiscard]] inline PackedEdge ek(int a, int b) const noexcept {
        return pack_edge(a, b);
    }
    [[nodiscard]] inline bool is_constrained(int a, int b) const noexcept {
        return constrained_keys_.find(ek(a,b)) != constrained_keys_.end();
    }
    inline void add_constraint(int a, int b) {
        const auto k = ek(a,b);
        if (constrained_keys_.insert(k).second) constraints_.push_back({a,b});
    }
    
public:
    DelaunayTriangulator();
    DelaunayTriangulator(const DelaunayTriangulationConfig& cfg);
    
    DelaunayTriangulationResult triangulate(const std::vector<Point2D>& input_points);
    
    DelaunayTriangulationResult triangulate_with_boundary(const std::vector<Point2D>& input_points,
                                   const std::vector<Point2D>& boundary);
    
    DelaunayTriangulationResult triangulate_polygon(const std::vector<Point2D>& polygon);

    DelaunayTriangulationResult triangulate_with_boundaries(const std::vector<Point2D>& input_points,
                                   const std::vector<std::vector<Point2D>>& loops_ccw_outer_cw_holes);
    
    void set_config(const DelaunayTriangulationConfig& cfg) { config = cfg; }
    const DelaunayTriangulationConfig& get_config() const { return config; }
    
    // Validation
    static bool validate_triangulation(const DelaunayTriangulationResult& result, double eps = 1e-12);

    void set_density_function(std::shared_ptr<DensityFunction> f);
    
    static void export_csv(const DelaunayTriangulationResult& result, 
                          const std::string& nodes_file,
                          const std::string& triangles_file);

    void lloyd_smoothing();
    void edge_flipping_pass();
    void refine_min_angle(double min_deg, int max_steiner = 1000);

    DelaunayTriangulationResult build_result_from_state() const;
    int find_blocking_vertex_on_segment(int a, int b) const;

    DelaunayTriangulationResult triangulate_circle(const std::vector<Point2D>& input_points);
};

}

#endif // DELAUNAY2D_H