#ifndef FEM_FRACTAL_DOMAIN_GENERATOR_H
#define FEM_FRACTAL_DOMAIN_GENERATOR_H

#include "delaunay_types.h"
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>

namespace fem {

struct FractalDomainConfig;

class FractalDomainGenerator {
public:
    static std::optional<std::string> generate(
        const FractalDomainConfig& cfg,
        std::vector<Point2D>& out_points
    );

    static std::optional<std::string> generate_boundary_loops(
        const FractalDomainConfig& cfg,
        std::vector<Point2D>& out_outer,
        std::vector<std::vector<Point2D>>& out_holes
    );

    using Polyline = std::vector<glm::dvec2>;
private:

    static void polyline_to_points(const Polyline& poly, std::vector<Point2D>& out_points);
    static Polyline make_square_ccw(glm::dvec2 center, double half_size);
    static void ensure_cw(Polyline& poly);

    static Polyline make_base_polygon(const FractalDomainConfig& cfg);
    static void refine_koch_snowflake(Polyline& poly, int iterations);
    static void refine_quadratic_koch(Polyline& poly, int iterations);
    static void refine_minkowski(Polyline& poly, int iterations);
    static void midpoint_displacement_loop(Polyline& poly, int iterations, uint32_t seed, double roughness);

    static Polyline resample_closed_by_arclength(const Polyline& poly, int sample_count);

    static double signed_area(const Polyline& poly);
    static void ensure_ccw(Polyline& poly);
    static void drop_duplicate_last(Polyline& poly, double eps = 1e-12);
};

} // namespace fem

#endif
