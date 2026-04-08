#include "planar_mesh_inner_boundary.h"

namespace fem {

#define SHOW_WHEN_EDITING_ENABLED()   \
    SHOW_WHEN_MEMBER(PlanarMeshInnerBoundary, is_editing_enabled_, val)

FEM_DEFINE_OBJECT(PlanarMeshInnerBoundary, PlanarMeshBoundaryBase, NoTypeHeader());
FEM_BEGIN_PROPERTY_REGISTER(PlanarMeshInnerBoundary)
{
    FEM_REGISTER_PROPERTY(
        PlanarMeshInnerBoundary, 
        input_type_, 
        DisplayName("Boundary Input"));

    FEM_REGISTER_PROPERTY(
        PlanarMeshInnerBoundary,
        smooth_stroke_config_,
        NoTypeHeader(),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::SmoothStroke),
        ON_VALUE_CHANGED(PlanarMeshInnerBoundary, recreate_smooth_stroke)
    );

    FEM_REGISTER_PROPERTY(
        PlanarMeshInnerBoundary, 
        parametric_curve_config_, 
        NoTypeHeader(), 
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Parametric),
        ON_VALUE_CHANGED(PlanarMeshInnerBoundary, generate_parametric_curve)
    );
    
    FEM_REGISTER_PROPERTY(
        PlanarMeshInnerBoundary,
        fractal_domain_config_,
        NoTypeHeader(),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Fractal),
        ON_VALUE_CHANGED(PlanarMeshInnerBoundary, generate_fractal_domain)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshInnerBoundary, 
        enable_editing, 
        SHOW_WHEN_MEMBER(
            PlanarMeshInnerBoundary,
            is_editing_enabled_,
            !val
        ),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Polygon, BoundaryInputType::SmoothStroke)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshInnerBoundary, 
        disable_editing, 
        SHOW_WHEN_EDITING_ENABLED(),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Polygon, BoundaryInputType::SmoothStroke)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshInnerBoundary,
        generate_parametric_curve,
        DisplayName("Generate"),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Parametric)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshInnerBoundary,
        generate_fractal_domain,
        DisplayName("Generate"),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Fractal)
    );

    FEM_REGISTER_PROPERTY(PlanarMeshInnerBoundary, points_, NoUI());
    FEM_REGISTER_PROPERTY(PlanarMeshInnerBoundary, smooth_stroke_init_points_, NoUI());
}
FEM_END_PROPERTY_REGISTER(PlanarMeshInnerBoundary)

PlanarMeshInnerBoundary::PlanarMeshInnerBoundary() {
    input_type_ = BoundaryInputType::SmoothStroke;
}

void PlanarMeshInnerBoundary::on_input_type_changed() {
    // No Polygon input support for inner boundary for now.
    if (input_type_ == BoundaryInputType::Polygon) {
        input_type_ = BoundaryInputType::SmoothStroke;
    }
}

}