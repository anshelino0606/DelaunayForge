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

namespace fem::detail {

struct TriPool final {
    using id_type = int32_t;
    static constexpr id_type kInvalid = -1;

    using value_type = Tri;
    using iterator = std::vector<Tri>::iterator;
    using const_iterator = std::vector<Tri>::const_iterator;

    iterator begin() noexcept { return tris_.begin(); }
    iterator end()   noexcept { return tris_.end(); }
    const_iterator begin() const noexcept { return tris_.begin(); }
    const_iterator end()   const noexcept { return tris_.end(); }

    [[nodiscard]] std::size_t size() const noexcept { return tris_.size(); }
    [[nodiscard]] bool empty() const noexcept { return tris_.empty(); }

    TriPool& operator=(const std::vector<Tri>& vec) {
        tris_ = vec;
        free_.clear();
        return *this;
    }
    TriPool& operator=(std::vector<Tri>&& vec) noexcept {
        tris_ = std::move(vec);
        free_.clear();
        return *this;
    }

    std::vector<Tri> as_vector() const { return tris_; }


    TriPool() = default;

    void clear() noexcept {
        tris_.clear();
        free_.clear();
    }

    void reserve(std::size_t n_tris) {
        tris_.reserve(n_tris);
        free_.reserve(n_tris / 2);
    }

    [[nodiscard]] Tri& operator[](id_type id) noexcept {
        return tris_[static_cast<std::size_t>(id)];
    }
    [[nodiscard]] const Tri& operator[](id_type id) const noexcept {
        return tris_[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] bool alive(id_type id) const noexcept {
        if (id < 0) return false;
        const auto u = static_cast<std::size_t>(id);
        return u < tris_.size() && tris_[u].valid;
    }

    /// Allocate a triangle slot (reuses erased ids first).
    [[nodiscard]] id_type alloc(int a, int b, int c) {
        if (!free_.empty()) {
            const id_type id = free_.back();
            free_.pop_back();
            Tri& t = tris_[static_cast<std::size_t>(id)];
            t = Tri(a, b, c, id);
            t.valid = true;
            return id;
        }
        const id_type id = static_cast<id_type>(tris_.size());
        tris_.emplace_back(a, b, c, id);
        tris_.back().valid = true;
        return id;
    }

    /// Mark a triangle dead and make its id reusable.
    void erase(id_type id) noexcept {
        if (id < 0) return;
        const auto u = static_cast<std::size_t>(id);
        if (u >= tris_.size()) return;

        Tri& t = tris_[u];
        if (!t.valid) return;

        t.valid = false;
        free_.push_back(id);
    }

    /// Make storage dense again and re-assign ids [0..m-1].
    template <class KeepPred>
    void compact_keep_if(KeepPred&& keep) {
        std::vector<Tri> dense;
        dense.reserve(tris_.size());

        for (const Tri& src : tris_) {
            if (!src.valid) continue;
            if (!std::invoke(keep, src)) continue;

            Tri dst = src;
            dst.id = static_cast<int>(dense.size());
            dst.valid = true;
            dst.neighbors[0] = dst.neighbors[1] = dst.neighbors[2] = -1;
            dense.push_back(dst);
        }

        tris_.swap(dense);
        free_.clear();
    }

    /// keep all currently-valid triangles, just densify.
    void compact_valid_only() {
        compact_keep_if([](const Tri&) { return true; });
    }



private:
    std::vector<Tri>     tris_;
    std::vector<id_type> free_;
};;


using EdgeKey = std::uint64_t;

[[nodiscard]] constexpr EdgeKey pack_edge(int a, int b) noexcept {
    const std::uint32_t ua = static_cast<std::uint32_t>(a < b ? a : b);
    const std::uint32_t ub = static_cast<std::uint32_t>(a < b ? b : a);
    return (static_cast<EdgeKey>(ua) << 32) | static_cast<EdgeKey>(ub);
}

struct U64Hash {
    [[nodiscard]] std::size_t operator()(EdgeKey k) const noexcept {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return static_cast<std::size_t>(k);
    }
};

[[nodiscard]] constexpr bool valid_index(int v, std::size_t n) noexcept {
    return v >= 0 && static_cast<std::size_t>(v) < n;
}

[[nodiscard]] inline double hypot2(double dx, double dy) noexcept {
    return std::hypot(dx, dy);
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
    detail::TriPool triangles;

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
    std::unordered_set<detail::EdgeKey, detail::U64Hash> constrained_keys_;
    std::vector<std::pair<int,int>> constraints_;     
    std::vector<std::vector<int>> active_boundary_loops_;
    std::vector<LoopBBox> active_loop_bboxes_;

    
    std::unordered_map<detail::EdgeKey, int, detail::U64Hash> edge_index_cache_;

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

    [[nodiscard]] inline detail::EdgeKey ek(int a, int b) const noexcept {
    return detail::pack_edge(a, b);
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