#ifndef FEM_PARAMETRIC_CURVE_H
#define FEM_PARAMETRIC_CURVE_H

#include "geometry_2d.h"
#include <string>
#include <optional>

namespace fem {

struct ParametricCurveConfig;

class ParametricCurveGenerator {
public:
    static std::optional<std::string> generate(
        const ParametricCurveConfig& config,
        std::vector<Point2D>& out_points
    );

private:
    static glm::dvec2 evaluate_circle(double t, const ParametricCurveConfig& cfg);
    static glm::dvec2 evaluate_ellipse(double t, const ParametricCurveConfig& cfg);
    static glm::dvec2 evaluate_cardioid(double t, const ParametricCurveConfig& cfg);
    static glm::dvec2 evaluate_lemniscate(double t, const ParametricCurveConfig& cfg);
    static glm::dvec2 evaluate_epicycloid(double t, const ParametricCurveConfig& cfg);
    static glm::dvec2 evaluate_hypocycloid(double t, const ParametricCurveConfig& cfg);
    static glm::dvec2 evaluate_spiral(double t, const ParametricCurveConfig& cfg);

    using ParametricFunc = std::function<glm::dvec2(double)>;
    static ParametricFunc get_preset_function(const ParametricCurveConfig& cfg);

    static std::optional<std::string> generate_custom(
        const ParametricCurveConfig& cfg,
        std::vector<Point2D>& out_points
    );
    
    struct ExprContext {
        double t, a, b, c;
    };
};

}

#endif // FEM_PARAMETRIC_CURVE_H