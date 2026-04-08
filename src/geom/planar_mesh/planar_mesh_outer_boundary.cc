#include "planar_mesh_outer_boundary.h"
#include "planar_mesh_component.h"
#include "geom/fractal_domain_generator.h"
#include "log_categories.h"
#include <random>

namespace fem {

#define SHOW_WHEN_EDITING_ENABLED()   \
    SHOW_WHEN_MEMBER(PlanarMeshOuterBoundary, is_editing_enabled_, val)

FEM_DEFINE_OBJECT(PlanarMeshOuterBoundary, PlanarMeshBoundaryBase, NoTypeHeader());
FEM_BEGIN_PROPERTY_REGISTER(PlanarMeshOuterBoundary)
{
    FEM_REGISTER_PROPERTY(
        PlanarMeshOuterBoundary, 
        input_type_, 
        DisplayName("Boundary Input"));

    FEM_REGISTER_PROPERTY(
        PlanarMeshOuterBoundary,
        smooth_stroke_config_,
        NoTypeHeader(),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::SmoothStroke),
        ON_VALUE_CHANGED(PlanarMeshOuterBoundary, recreate_smooth_stroke)
    );

    FEM_REGISTER_PROPERTY(
        PlanarMeshOuterBoundary, 
        parametric_curve_config_, 
        NoTypeHeader(), 
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Parametric),
        ON_VALUE_CHANGED(PlanarMeshOuterBoundary, generate_parametric_curve)
    );

    FEM_REGISTER_PROPERTY(
    PlanarMeshOuterBoundary,
        fractal_domain_config_,
        NoTypeHeader(),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Fractal),
        ON_VALUE_CHANGED(PlanarMeshOuterBoundary, generate_fractal_domain)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshOuterBoundary, 
        enable_editing, 
        SHOW_WHEN_MEMBER(
            PlanarMeshOuterBoundary,
            is_editing_enabled_,
            !val
        ),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Polygon, BoundaryInputType::SmoothStroke)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshOuterBoundary, 
        disable_editing, 
        SHOW_WHEN_EDITING_ENABLED(),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Polygon, BoundaryInputType::SmoothStroke)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshOuterBoundary, 
        generate_random_points,
        SHOW_WHEN_EDITING_ENABLED(),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Polygon),
        SameLine()
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshOuterBoundary, 
        generate_grid,
        SHOW_WHEN_EDITING_ENABLED(),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Polygon),
        SameLine()
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshOuterBoundary,
        generate_parametric_curve,
        DisplayName("Generate"),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Parametric)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshOuterBoundary,
        generate_fractal_domain,
        DisplayName("Generate"),
        SHOW_FOR_ENUM(input_type_, BoundaryInputType::Fractal)
    );

    FEM_REGISTER_PROPERTY(PlanarMeshOuterBoundary, points_, NoUI());
    FEM_REGISTER_PROPERTY(PlanarMeshOuterBoundary, smooth_stroke_init_points_, NoUI());
}
FEM_END_PROPERTY_REGISTER(PlanarMeshOuterBoundary)

// Those constants are area where points will be generated.
// In previous impl canvas size was used for that.
// Now it's impossible to pass canvas size to PlanarMeshComponent so I decided to use some constants
constexpr size_t g_size_x = 960;
constexpr size_t g_size_y = 650;
constexpr size_t g_random_point_count = 50;
constexpr size_t g_grid_size_x = 8;
constexpr size_t g_grid_size_y = 6;

void PlanarMeshOuterBoundary::generate_random_points() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> x_dist(20.0f, g_size_x);
    std::uniform_real_distribution<float> y_dist(20.0f, g_size_y);
    
    mesh_component()->reset();
    
    for (int i = 0; i < g_random_point_count; ++i) {
        points_.emplace_back(x_dist(gen), y_dist(gen), (int)points_.size());
    }

    mesh_component()->triangulate();
}

void PlanarMeshOuterBoundary::generate_grid() {
    float dx = (float)g_size_x  / std::max(1ull, static_cast<unsigned long long>(g_grid_size_x - 1));
    float dy = (float)g_size_y / std::max(1ull, static_cast<unsigned long long>(g_grid_size_y - 1));

    mesh_component()->reset();
    
    for (size_t j = 0; j < g_grid_size_y; ++j) {
        for (size_t i = 0; i < g_grid_size_x; ++i) {
            float x = 20.0f + i * dx;
            float y = 20.0f + j * dy;
            points_.emplace_back(x, y, (int)points_.size());
        }
    }

    mesh_component()->triangulate();
}

void PlanarMeshOuterBoundary::generate_fractal_domain() {
    if (fractal_domain_config_.preset.value != FractalPreset::SierpinskiCarpet) {
        PlanarMeshBoundaryBase::generate_fractal_domain();
        return;
    }

    std::vector<Point2D> outer;
    std::vector<std::vector<Point2D>> holes;
    if (auto err = FractalDomainGenerator::generate_boundary_loops(fractal_domain_config_, outer, holes)) {
        LOGT_ERROR(LogGeometry, "Fractal generation failed: %s", err->c_str());
        return;
    }

    PlanarMeshComponent* mc = mesh_component();
    mc->begin_bulk_update();
    set_points(outer);

    mc->replace_inner_boundaries(std::move(holes));

    mc->end_bulk_update(true);
}

}