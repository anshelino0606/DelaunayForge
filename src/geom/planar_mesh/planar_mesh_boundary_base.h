#ifndef FEM_PLANAR_MESH_BOUNDARY_BASE_H
#define FEM_PLANAR_MESH_BOUNDARY_BASE_H

#include "core/object/object.h"
#include "core/object/property.h"
#include "geom/delaunay_types.h"
#include "geom/smooth_stroke_config.h"
#include "geom/parametric_curve_config.h"
#include "geom/fractal_domain_config.h"

namespace fem {

class PlanarMeshComponent;

FEM_DECLARE_ENUM(BoundaryInputType, Polygon, SmoothStroke, Parametric, Fractal);

class PlanarMeshBoundaryBase : public Object {
public:
    FEM_DECLARE_OBJECT(PlanarMeshBoundaryBase);
    FEM_DECLARE_PROPERTY_REGISTER(PlanarMeshBoundaryBase);

    void set_points(std::vector<Point2D>& points);
    void add_point(glm::dvec2 position);
    void clear_points();

    void enable_editing();
    void disable_editing();

    void set_is_editing_enabled(bool value) {
        is_editing_enabled_ = value;
    }

    bool is_editing_enabled() const { return is_editing_enabled_; }

    PlanarMeshComponent* mesh_component() const;

    const std::vector<Point2D>& points() const { return points_; }
    const BoundaryInputType input_type() const { return input_type_; }
    const SmoothStrokeConfig& smooth_stroke_config() const { return smooth_stroke_config_; }
    
    void set_input_type(BoundaryInputType input_type) {
        input_type_ = input_type;
    }
    
    void set_smooth_stroke_config(const SmoothStrokeConfig& config) {
        smooth_stroke_config_ = config;
    }
    
protected:
    std::vector<Point2D> points_;
    std::vector<Point2D> smooth_stroke_init_points_;
    BoundaryInputType input_type_ = BoundaryInputType::Polygon;
    SmoothStrokeConfig smooth_stroke_config_;
    ParametricCurveConfig parametric_curve_config_;
    FractalDomainConfig fractal_domain_config_;

    bool is_editing_enabled_ = false;

    void recreate_smooth_stroke();
    void generate_parametric_curve();
    void generate_fractal_domain();
};

}

#endif // FEM_PLANAR_MESH_BOUNDARY_BASE_H