#ifndef PARAMETRIC_CURVE_TOOL_H
#define PARAMETRIC_CURVE_TOOL_H

#include <vector>
#include <string>
#include <functional>
#include <optional>
#include <glm/glm.hpp>
#include "geom/delaunay/delaunay_types.h"
#include "math/expression_parser.h"
#include "geom/parametric_curve_config.h"

namespace fem {

// enum class ParametricPreset {
//     Circle,
//     Ellipse,
//     Cardioid,
//     Lemniscate,
//     Epicycloid,
//     Hypocycloid,
//     Rose,
//     Spiral,
//     Custom
// };

// struct ParametricCurveConfig {
//     ParametricPreset preset = ParametricPreset::Circle;
    
//     // Parameters
//     double a = 100.0;
//     double b = 100.0;
//     double c = 1.0;
    
//     double t_start = 0.0;
//     double t_end = 2.0 * 3.14159265358979323846;
    
//     int sample_count = 128;
    
//     glm::dvec2 center{0.0, 0.0};
    
//     std::string custom_x_expr;
//     std::string custom_y_expr;
// };

class ParametricCurveTool {
public:
    ParametricCurveTool() = default;
    
    std::vector<Point2D> generate(const ParametricCurveConfig& cfg);
    
    using ParametricFunc = std::function<glm::dvec2(double)>;
    ParametricFunc get_preset_function(const ParametricCurveConfig& cfg) const;
    
    const std::string& last_error() const { return last_error_; }

    std::optional<std::vector<Point2D>> generate_custom(
        const std::string& x_expr,
        const std::string& y_expr,
        double t_start,
        double t_end,
        int sample_count,
        const glm::dvec2& center,
        double a, double b, double c
    );
    
private:

    glm::dvec2 evaluate_circle(double t, const ParametricCurveConfig& cfg) const;
    glm::dvec2 evaluate_ellipse(double t, const ParametricCurveConfig& cfg) const;
    glm::dvec2 evaluate_cardioid(double t, const ParametricCurveConfig& cfg) const;
    glm::dvec2 evaluate_lemniscate(double t, const ParametricCurveConfig& cfg) const;
    glm::dvec2 evaluate_epicycloid(double t, const ParametricCurveConfig& cfg) const;
    glm::dvec2 evaluate_hypocycloid(double t, const ParametricCurveConfig& cfg) const;
    glm::dvec2 evaluate_spiral(double t, const ParametricCurveConfig& cfg) const;
    
    struct ExprContext {
        double t, a, b, c;
    };

    ExpressionParser parser_;
    std::string last_error_;
    
    std::optional<double> eval_expr(const std::string& expr, const ExprContext& ctx) const;
};

} // namespace fem

#endif