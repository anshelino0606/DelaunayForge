#ifndef FEM_FRACTAL_DOMAIN_CONFIG_H
#define FEM_FRACTAL_DOMAIN_CONFIG_H

#include "core/object/object.h"
#include "core/object/property.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace fem {

FEM_DECLARE_ENUM(
    FractalPreset,
    KochSnowflake,
    QuadraticKochIsland,
    MinkowskiIsland,
    MidpointDisplacementLoop,
    SierpinskiCarpet,
    MandelbrotBoundary,
    JuliaSetBoundary
);

struct FractalDomainConfig : public Struct {
    FEM_DECLARE_STRUCT(FractalDomainConfig);
    FEM_DECLARE_PROPERTY_REGISTER(FractalDomainConfig);

    FractalPreset preset = FractalPreset::KochSnowflake;

    double size = 200.0;
    uint32_t seed = 1;
    double roughness = 0.7;

    int iterations = 4;
    int sample_count = 512;

    glm::dvec2 center{450.0, 300.0};

    // Koch snowflake
    double snowflake_side_length = 200.0;

    // Sierpinski triangle
    double triangle_size = 200.0;

    // Dragon curve
    double dragon_segment_length = 5.0;

    // Hilbert curve
    double hilbert_size = 200.0;

    // Mandelbrot boundary extraction
    double mandelbrot_bound = 2.0;
    int mandelbrot_resolution = 100;

    // Julia set boundary extraction
    glm::dvec2 julia_c{-0.7, 0.27};
    double julia_bound = 2.0;
    int julia_resolution = 100;

    int max_iterations = 50;

    int carpet_max_holes = 5000;
    double carpet_min_hole_half_size = 0.0;

    double cantor_width = 300.0;
    double cantor_height = 20.0;

    // Lévy C curve
    double levy_length = 200.0;

    // Peano curve
    double peano_size = 200.0;

private:
    void on_preset_changed();
};

} // namespace fem

#endif
