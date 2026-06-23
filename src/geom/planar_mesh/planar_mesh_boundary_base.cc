#include "planar_mesh_boundary_base.h"
#include "planar_mesh_component.h"
#include "math/curve.h"
#include "geom/generators/parametric_curve_generator.h"
#include "geom/generators/fractal_domain_generator.h"
#include "log_categories.h"

namespace fem {

FEM_DEFINE_OBJECT(PlanarMeshBoundaryBase, Object, NoTypeHeader());
FEM_BEGIN_PROPERTY_REGISTER(PlanarMeshBoundaryBase)
{

}
FEM_END_PROPERTY_REGISTER(PlanarMeshBoundaryBase);

FEM_DEFINE_ENUM(BoundaryInputType, DrawAsToggles(true));

void PlanarMeshBoundaryBase::set_points(std::vector<Point2D>& points) {
    clear_points();

    points_ = std::move(points);

    for (size_t i = 0; i != points_.size(); ++i) {
        Point2D& point = points_[i];
        point.on_boundary = true;   
    }

    if (input_type_ == BoundaryInputType::SmoothStroke) {
        smooth_stroke_init_points_ = points_;
    }

    mesh_component()->request_triangulate();
}

void PlanarMeshBoundaryBase::add_point(glm::dvec2 position) {
    points_.emplace_back(position.x, position.y, (int)points_.size());
    mesh_component()->request_triangulate();
}

void PlanarMeshBoundaryBase::clear_points() {
    points_.clear();
    smooth_stroke_init_points_.clear();
}

void PlanarMeshBoundaryBase::enable_editing() {
    is_editing_enabled_ = true;
    mesh_component()->set_edited_boundary(this);
}

void PlanarMeshBoundaryBase::disable_editing() {
    is_editing_enabled_ = false;
    mesh_component()->set_edited_boundary(nullptr);
}

PlanarMeshComponent* PlanarMeshBoundaryBase::mesh_component() const {
    return static_cast<PlanarMeshComponent*>(owner_);
}

void PlanarMeshBoundaryBase::recreate_smooth_stroke() {
    std::vector<glm::dvec2> world_positions;
    world_positions.reserve(smooth_stroke_init_points_.size());
    for (const Point2D& point : smooth_stroke_init_points_) {
        world_positions.push_back(point.p);
    }

    points_.clear();

    points_ = make_closed_smooth_loop(
        std::move(world_positions),
        smooth_stroke_config_.boundary_sample_count,
        smooth_stroke_config_.close_threshold_px,
        smooth_stroke_config_.min_dist_px,
        smooth_stroke_config_.catmull_per_seg
    );

    mesh_component()->request_triangulate();
}

void PlanarMeshBoundaryBase::generate_parametric_curve() {
    clear_points();

    ParametricCurveGenerator::generate(parametric_curve_config_, points_);
    mesh_component()->request_triangulate();
}

void PlanarMeshBoundaryBase::generate_fractal_domain() {
    clear_points();

    std::vector<Point2D> generated;
    if (auto err = FractalDomainGenerator::generate(fractal_domain_config_, generated)) {
        LOGT_ERROR(LogGeometry, "Fractal generation failed: %s", err->c_str());
        return;
    }

    points_ = std::move(generated);
    mesh_component()->request_triangulate();
}

}