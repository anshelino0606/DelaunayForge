#include "parametric_curve.h"
#include "log_categories.h"
#include <cmath>

namespace fem {

std::vector<Point2D> ParametricCurveTool::generate(const ParametricCurveConfig& cfg) {
    last_error_.clear();

    if (cfg.sample_count < 3) {
        last_error_ = "sample_count must be >= 3";
        return {};
    }

    if (cfg.preset == ParametricPreset::Custom) {
        auto result = generate_custom(
            cfg.custom_x_expr,
            cfg.custom_y_expr,
            cfg.t_start,
            cfg.t_end,
            cfg.sample_count,
            cfg.center,
            cfg.a, cfg.b, cfg.c
        );
        if (!result) {
            if (last_error_.empty()) last_error_ = "Failed to generate custom curve";
            return {};
        }
        return *result;
    }

    auto func = get_preset_function(cfg);

    std::vector<Point2D> points;
    points.reserve(cfg.sample_count);

    const double dt = (cfg.t_end - cfg.t_start) / cfg.sample_count; // no endpoint duplication

    for (int i = 0; i < cfg.sample_count; ++i) {
        double t = cfg.t_start + i * dt;
        glm::dvec2 pos = func(t);

        Point2D pt;
        pt.p[0] = pos.x;
        pt.p[1] = pos.y;
        pt.id = i;
        pt.on_boundary = true;

        points.push_back(pt);
    }

    return points;
}

ParametricCurveTool::ParametricFunc 
ParametricCurveTool::get_preset_function(const ParametricCurveConfig& cfg) const {
    // switch (cfg.preset) {
    //     case ParametricPreset::Circle:
    //         return [this, cfg](double t) { return evaluate_circle(t, cfg); };
    //     case ParametricPreset::Ellipse:
    //         return [this, cfg](double t) { return evaluate_ellipse(t, cfg); };
    //     case ParametricPreset::Cardioid:
    //         return [this, cfg](double t) { return evaluate_cardioid(t, cfg); };
    //     case ParametricPreset::Lemniscate:
    //         return [this, cfg](double t) { return evaluate_lemniscate(t, cfg); };
    //     case ParametricPreset::Epicycloid:
    //         return [this, cfg](double t) { return evaluate_epicycloid(t, cfg); };
    //     case ParametricPreset::Hypocycloid:
    //         return [this, cfg](double t) { return evaluate_hypocycloid(t, cfg); };
    //     case ParametricPreset::Rose:
    //         return [this, cfg](double t) { return evaluate_rose(t, cfg); };
    //     case ParametricPreset::Spiral:
    //         return [this, cfg](double t) { return evaluate_spiral(t, cfg); };
    //     default:
    //         return [this, cfg](double t) { return evaluate_circle(t, cfg); };
    // }
    return {};
}

glm::dvec2 ParametricCurveTool::evaluate_circle(double t, const ParametricCurveConfig& cfg) const {
    return cfg.center + glm::dvec2(
        cfg.a * std::cos(t),
        cfg.a * std::sin(t)
    );
}

glm::dvec2 ParametricCurveTool::evaluate_ellipse(double t, const ParametricCurveConfig& cfg) const {
    return cfg.center + glm::dvec2(
        cfg.a * std::cos(t),
        cfg.b * std::sin(t)
    );
}

glm::dvec2 ParametricCurveTool::evaluate_cardioid(double t, const ParametricCurveConfig& cfg) const {
    double r = cfg.a * (1.0 - std::cos(t));
    return cfg.center + glm::dvec2(
        r * std::cos(t),
        r * std::sin(t)
    );
}

glm::dvec2 ParametricCurveTool::evaluate_lemniscate(double t, const ParametricCurveConfig& cfg) const {
    double sin_t = std::sin(t);
    double cos_t = std::cos(t);
    double denom = 1.0 + sin_t * sin_t;
    
    if (std::abs(denom) < 1e-10) {
        return cfg.center;
    }
    
    return cfg.center + glm::dvec2(
        cfg.a * cos_t / denom,
        cfg.a * sin_t * cos_t / denom
    );
}

glm::dvec2 ParametricCurveTool::evaluate_epicycloid(double t, const ParametricCurveConfig& cfg) const {
    double ratio = (cfg.a + cfg.b) / std::max(1e-6, cfg.b);
    return cfg.center + glm::dvec2(
        (cfg.a + cfg.b) * std::cos(t) - cfg.b * std::cos(ratio * t),
        (cfg.a + cfg.b) * std::sin(t) - cfg.b * std::sin(ratio * t)
    );
}

glm::dvec2 ParametricCurveTool::evaluate_hypocycloid(double t, const ParametricCurveConfig& cfg) const {
    double ratio = (cfg.a - cfg.b) / std::max(1e-6, cfg.b);
    return cfg.center + glm::dvec2(
        (cfg.a - cfg.b) * std::cos(t) + cfg.b * std::cos(ratio * t),
        (cfg.a - cfg.b) * std::sin(t) - cfg.b * std::sin(ratio * t)
    );
}

glm::dvec2 ParametricCurveTool::evaluate_spiral(double t, const ParametricCurveConfig& cfg) const {
    double r = cfg.a + cfg.b * t;
    return cfg.center + glm::dvec2(
        r * std::cos(t),
        r * std::sin(t)
    );
}

std::optional<std::vector<Point2D>> ParametricCurveTool::generate_custom(
    const std::string& x_expr,
    const std::string& y_expr,
    double t_start,
    double t_end,
    int sample_count,
    const glm::dvec2& center,
    double a, double b, double c
) {
    last_error_.clear();

    if (x_expr.empty() || y_expr.empty() || sample_count < 3) {
        last_error_ = "Empty expression or sample_count < 3";
        return std::nullopt;
    }

    if (!parser_.validate(x_expr)) {
        last_error_ = "Invalid x expression: " + parser_.last_error();
        LOGT_ERROR(LogMath, "Invalid x expression: %s (error: %s)",
                   x_expr.c_str(), parser_.last_error().c_str());
        return std::nullopt;
    }

    if (!parser_.validate(y_expr)) {
        last_error_ = "Invalid y expression: " + parser_.last_error();
        LOGT_ERROR(LogMath, "Invalid y expression: %s (error: %s)",
                   y_expr.c_str(), parser_.last_error().c_str());
        return std::nullopt;
    }

    std::vector<Point2D> points;
    points.reserve(sample_count);

    const double dt = (t_end - t_start) / sample_count; // avoids duplicating endpoint

    std::unordered_map<std::string, double> vars;
    vars["a"] = a;
    vars["b"] = b;
    vars["c"] = c;
    vars["pi"] = Math::PI;
    vars["e"] = Math::E;

    for (int i = 0; i < sample_count; ++i) {
        double t = t_start + i * dt;
        vars["t"] = t;

        auto x_val = parser_.evaluate(x_expr, vars);
        if (!x_val) {
            last_error_ = "x(t) evaluation failed: " + parser_.last_error();
            LOGT_ERROR(LogMath, "Failed to evaluate x expression at t=%f: %s",
                       t, parser_.last_error().c_str());
            return std::nullopt;
        }

        auto y_val = parser_.evaluate(y_expr, vars);
        if (!y_val) {
            last_error_ = "y(t) evaluation failed: " + parser_.last_error();
            LOGT_ERROR(LogMath, "Failed to evaluate y expression at t=%f: %s",
                       t, parser_.last_error().c_str());
            return std::nullopt;
        }

        Point2D pt;
        pt.p[0] = center.x + *x_val;
        pt.p[1] = center.y + *y_val;
        pt.id = i;
        pt.on_boundary = true;

        points.push_back(pt);
    }

    LOGT_DEBUG(LogMath, "Successfully generated %d custom parametric points", sample_count);
    return points;
}

} // namespace fem