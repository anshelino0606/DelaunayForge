#ifndef FEM_PLANAR_MESH_COMPONENT_H
#define FEM_PLANAR_MESH_COMPONENT_H

#include "geom/mesh/mesh_component.h"
#include "geom/delaunay/delaunay_types.h"
#include "geom/planar_mesh/random_inner_boundary_config.h"
#include "math/density_config.h"
#include "math/density_functions.h"

namespace fem {

class BoundaryCondition;
class PlanarMeshOuterBoundary;
class PlanarMeshInnerBoundary;
class PlanarMeshBoundaryBase;
class PlanarMeshGenerator;

struct BoundaryLoopContainer : public Struct {
    FEM_DECLARE_STRUCT(BoundaryLoopContainer);
    FEM_DECLARE_PROPERTY_REGISTER(BoundaryLoopContainer);

    std::vector<Point2D> points;
};

class PlanarMeshComponent : public MeshComponent {
public:
    FEM_DECLARE_OBJECT(PlanarMeshComponent);
    FEM_DECLARE_PROPERTY_REGISTER(PlanarMeshComponent);

    PlanarMeshComponent();
    virtual ~PlanarMeshComponent() override;

    void add_user_point(glm::dvec2 pos);
    void add_polygon_point(glm::dvec2 pos);
    void add_boundary_point(glm::dvec2 pos);

    void add_inner_boundary(std::vector<Point2D>& points);
    void set_outer_boundary(std::vector<Point2D>& points);

    void reset();

    const std::vector<Point2D>& user_points() const { return user_points_; }
    const std::vector<Point2D>& boundary_points() const { return boundary_points_; }
    const std::vector<Point2D>& polygon_points() const { return polygon_points_; }
    const DensityConfig& density_config() const { return density_config_; }
    PlanarMeshOuterBoundary* outer_boundary() const { return outer_boundary_; }
    const std::vector<PlanarMeshInnerBoundary*>& inner_boundaries() const { return inner_boundaries_; }
    PlanarMeshBoundaryBase* edited_boundary() const { return edited_boundary_; }
    BoundaryCondition* edited_boundary_condition() const { return edited_boundary_condition_; }
    bool is_free_hand_enabled() const { return is_free_hand_enabled_; }

    void set_edited_boundary(PlanarMeshBoundaryBase* boundary);
    void reset_edited_boundary();

    void set_edited_boundary_condition(BoundaryCondition* boundary_condition);

    void triangulate();
    void request_triangulate();
    void update_triangulation(const DelaunayTriangulationResult& new_result);

    void begin_bulk_update();
    void end_bulk_update(bool triangulate_after = true);
    bool is_in_bulk_update() const { return bulk_update_depth_ > 0; }

    void replace_inner_boundaries(std::vector<std::vector<Point2D>> loops);
    void add_random_inner_boundaries();

    void clear_polygon_points() { polygon_points_.clear(); }
    void clear_boundary_points() { boundary_points_.clear(); }


    const std::vector<BoundaryLoopContainer>& boundary_loops() const { return boundary_loops_; }

    void set_polygon_points(std::vector<Point2D> pts, bool mark_boundary = true);
    void set_boundary_points(std::vector<Point2D> pts, bool mark_boundary = true);

    const DelaunayTriangulationResult& triangulation_result() const {
        return triangulation_result_;
    }

    virtual FEMMesh build_fem_mesh() const override;
    virtual void update_buffers() override;
    virtual uint32_t index_count() const override;

    void clear_boundary_loops() { boundary_loops_.clear(); }

    void set_single_boundary_loop(std::vector<Point2D> loop, bool mark_boundary = true) {
        clear_boundary_loops();
        add_boundary_loop(std::move(loop), mark_boundary);
    }

    void add_boundary_loop(std::vector<Point2D> loop, bool mark_boundary = true);

    std::shared_ptr<DensityFunction> build_density_function() const;

private:
    static void normalize_boundary_pts_(std::vector<Point2D>& pts, bool mark_boundary);

protected:
    std::vector<Point2D> user_points_;
    std::vector<Point2D> boundary_points_;
    std::vector<Point2D> polygon_points_;
    std::vector<BoundaryLoopContainer> boundary_loops_;

    PlanarMeshGenerator* mesh_generator_ = nullptr;
    PlanarMeshOuterBoundary* outer_boundary_ = nullptr;
    std::vector<PlanarMeshInnerBoundary*> inner_boundaries_;
    RandomInnerBoundaryConfig random_inner_boundary_config_;
    DensityConfig density_config_;

    PlanarMeshBoundaryBase* edited_boundary_ = nullptr;
    BoundaryCondition* edited_boundary_condition_ = nullptr;
    bool is_free_hand_enabled_ = false;

    mutable DelaunayTriangulationResult triangulation_result_;

    void clear_boundary_conditions();
    void clear_outer_boundary();
    void clear_inner_boundaries();

    void on_inner_boundary_added();
    void on_inner_boundary_pre_removed(void* elem);
    void on_inner_boundary_post_removed();

    void on_boundary_condition_pre_removed(void* elem);

    void enable_free_hand();
    void disable_free_hand();

private:
    int bulk_update_depth_ = 0;
    bool triangulation_dirty_ = false;
};

}

#endif // FEM_PLANAR_MESH_COMPONENT_H