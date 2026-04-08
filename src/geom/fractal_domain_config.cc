#include "fractal_domain_config.h"

namespace fem {

const double pi = 3.14159265358979323846;

FEM_DEFINE_ENUM(FractalPreset);
FEM_DEFINE_STRUCT(FractalDomainConfig);

FEM_BEGIN_PROPERTY_REGISTER(FractalDomainConfig)
{
    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        preset,
        ON_VALUE_CHANGED(FractalDomainConfig, on_preset_changed)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        iterations,
        DisplayName("Iterations"),
        ClampMin(0),
        ClampMax(8),
        SHOW_FOR_ENUM(preset,
            FractalPreset::KochSnowflake,
            FractalPreset::QuadraticKochIsland,
            FractalPreset::MinkowskiIsland,
            FractalPreset::MidpointDisplacementLoop,
            FractalPreset::SierpinskiCarpet)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        sample_count,
        DisplayName("Sample Count"),
        ClampMin(64),
        ClampMax(4096)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        center,
        ClampMin(0),
        ClampMax(5000.0f)
    );

    // Koch
    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        snowflake_side_length,
        DisplayName("Side Length"),
        ClampMin(20.0),
        ClampMax(800.0),
        SHOW_FOR_ENUM(preset, FractalPreset::KochSnowflake)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        size,
        DisplayName("Size"),
        ClampMin(10.0),
        ClampMax(1000.0),
        SHOW_FOR_ENUM(preset,
            FractalPreset::QuadraticKochIsland,
            FractalPreset::MinkowskiIsland,
            FractalPreset::MidpointDisplacementLoop,
            FractalPreset::SierpinskiCarpet)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        carpet_max_holes,
        DisplayName("Max Holes"),
        ClampMin(0),
        ClampMax(200000),
        SHOW_FOR_ENUM(preset, FractalPreset::SierpinskiCarpet)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        carpet_min_hole_half_size,
        DisplayName("Min Hole Half-Size"),
        ClampMin(0.0),
        ClampMax(1000.0),
        SHOW_FOR_ENUM(preset, FractalPreset::SierpinskiCarpet)
    );

    // Midpoint displacement parameters
    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        seed,
        DisplayName("Seed"),
        ClampMin(0),
        ClampMax(1000000),
        SHOW_FOR_ENUM(preset, FractalPreset::MidpointDisplacementLoop)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        roughness,
        DisplayName("Roughness"),
        ClampMin(0.0),
        ClampMax(1.0),
        SHOW_FOR_ENUM(preset, FractalPreset::MidpointDisplacementLoop)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        mandelbrot_bound,
        DisplayName("Boundary Size"),
        ClampMin(1.0),
        ClampMax(4.0),
        SHOW_FOR_ENUM(preset, FractalPreset::MandelbrotBoundary)
    );
    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        mandelbrot_resolution,
        DisplayName("Resolution"),
        ClampMin(50),
        ClampMax(800),
        SHOW_FOR_ENUM(preset, FractalPreset::MandelbrotBoundary)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        julia_c,
        DisplayName("Julia Parameter (c)"),
        ClampMin(-2.0),
        ClampMax(2.0),
        SHOW_FOR_ENUM(preset, FractalPreset::JuliaSetBoundary)
    );
    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        julia_bound,
        DisplayName("Boundary Size"),
        ClampMin(1.0),
        ClampMax(4.0),
        SHOW_FOR_ENUM(preset, FractalPreset::JuliaSetBoundary)
    );
    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        julia_resolution,
        DisplayName("Resolution"),
        ClampMin(50),
        ClampMax(800),
        SHOW_FOR_ENUM(preset, FractalPreset::JuliaSetBoundary)
    );

    FEM_REGISTER_PROPERTY(
        FractalDomainConfig,
        max_iterations,
        DisplayName("Max Iterations"),
        ClampMin(20),
        ClampMax(500),
        SHOW_FOR_ENUM(preset,
            FractalPreset::MandelbrotBoundary,
            FractalPreset::JuliaSetBoundary)
    );
}
FEM_END_PROPERTY_REGISTER(FractalDomainConfig)

void FractalDomainConfig::on_preset_changed() {
    switch (preset) {
        case FractalPreset::KochSnowflake:
            iterations = 4;
            snowflake_side_length = 200.0;
            break;

        case FractalPreset::QuadraticKochIsland:
            iterations = 3;
            size = 200.0;
            break;

        case FractalPreset::MinkowskiIsland:
            iterations = 3;
            size = 200.0;
            break;

        case FractalPreset::MidpointDisplacementLoop:
            iterations = 6;
            size = 200.0;
            seed = 1;
            roughness = 0.7;
            break;

        case FractalPreset::SierpinskiCarpet:
            iterations = 3;
            size = 200.0;
            carpet_max_holes = 5000;
            break;

        case FractalPreset::MandelbrotBoundary:
            mandelbrot_bound = 2.0;
            mandelbrot_resolution = 200;
            max_iterations = 50;
            break;

        case FractalPreset::JuliaSetBoundary:
            julia_c = glm::dvec2(-0.7, 0.27);
            julia_bound = 2.0;
            julia_resolution = 200;
            max_iterations = 50;
            break;
    }
}


} // namespace fem