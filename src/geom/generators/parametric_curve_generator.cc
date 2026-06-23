#include "parametric_curve_generator.h"
#include "math/expression_parser.h"
#include "geom/configs/parametric_curve_config.h"
#include "log_categories.h"

namespace fem {

std::optional<std::string> ParametricCurveGenerator::generate(
    const ParametricCurveConfig& config,
    std::vector<Point2D>& out_points
) {
    out_points.clear();
    out_points.reserve(config.sample_count);
    if (config.preset == ParametricPreset::Custom) {
        return generate_custom(config, out_points);
    }

    auto func = get_preset_function(config);

    out_points.reserve(config.sample_count);

    const double dt = (config.t_end - config.t_start) / config.sample_count; // no endpoint duplication

    for (int i = 0; i < config.sample_count; ++i) {
        double t = config.t_start + i * dt;
        glm::dvec2 pos = func(t);

        Point2D pt;
        pt.p[0] = pos.x;
        pt.p[1] = pos.y;
        pt.id = i;
        pt.on_boundary = true;

        out_points.push_back(pt);
    }

    return std::nullopt;
}

glm::dvec2 ParametricCurveGenerator::evaluate_circle(double t, const ParametricCurveConfig& cfg) {
    return cfg.center + glm::dvec2(
        cfg.radius * std::cos(t),
        cfg.radius * std::sin(t)
    );
}

glm::dvec2 ParametricCurveGenerator::evaluate_ellipse(double t, const ParametricCurveConfig& cfg) {
    return cfg.center + glm::dvec2(
        cfg.width * std::cos(t),
        cfg.height * std::sin(t)
    );
}

glm::dvec2 ParametricCurveGenerator::evaluate_cardioid(double t, const ParametricCurveConfig& cfg) {
    double r = cfg.size * (1.0 - std::cos(t));
    return cfg.center + glm::dvec2(
        r * std::cos(t),
        r * std::sin(t)
    );
}

glm::dvec2 ParametricCurveGenerator::evaluate_lemniscate(double t, const ParametricCurveConfig& cfg) {
    double sin_t = std::sin(t);
    double cos_t = std::cos(t);
    double denom = 1.0 + sin_t * sin_t;
    
    if (std::abs(denom) < 1e-10) {
        return cfg.center;
    }
    
    return cfg.center + glm::dvec2(
        cfg.size * cos_t / denom,
        cfg.size * sin_t * cos_t / denom
    );
}

glm::dvec2 ParametricCurveGenerator::evaluate_epicycloid(double t, const ParametricCurveConfig& cfg) {
    double ratio = (cfg.outer_radius + cfg.inner_radius) / std::max(1e-6, cfg.inner_radius);
    return cfg.center + glm::dvec2(
        (cfg.outer_radius + cfg.inner_radius) * std::cos(t) - cfg.inner_radius * std::cos(ratio * t),
        (cfg.outer_radius + cfg.inner_radius) * std::sin(t) - cfg.inner_radius * std::sin(ratio * t)
    );
}

glm::dvec2 ParametricCurveGenerator::evaluate_hypocycloid(double t, const ParametricCurveConfig& cfg) {
    double ratio = (cfg.outer_radius - cfg.inner_radius) / std::max(1e-6, cfg.inner_radius);
    return cfg.center + glm::dvec2(
        (cfg.outer_radius - cfg.inner_radius) * std::cos(t) + cfg.inner_radius * std::cos(ratio * t),
        (cfg.outer_radius - cfg.inner_radius) * std::sin(t) - cfg.inner_radius * std::sin(ratio * t)
    );
}

glm::dvec2 ParametricCurveGenerator::evaluate_spiral(double t, const ParametricCurveConfig& cfg) {
    double r = cfg.start_radius + cfg.growth_rate * t;
    return cfg.center + glm::dvec2(
        r * std::cos(t),
        r * std::sin(t)
    );
}

ParametricCurveGenerator::ParametricFunc ParametricCurveGenerator::get_preset_function(
    const ParametricCurveConfig& cfg
) {
    switch (cfg.preset.value) {
        case ParametricPreset::Circle:
            return [cfg](double t) { return evaluate_circle(t, cfg); };
        case ParametricPreset::Ellipse:
            return [cfg](double t) { return evaluate_ellipse(t, cfg); };
        case ParametricPreset::Cardioid:
            return [cfg](double t) { return evaluate_cardioid(t, cfg); };
        case ParametricPreset::Lemniscate:
            return [cfg](double t) { return evaluate_lemniscate(t, cfg); };
        case ParametricPreset::Epicycloid:
            return [cfg](double t) { return evaluate_epicycloid(t, cfg); };
        case ParametricPreset::Hypocycloid:
            return [cfg](double t) { return evaluate_hypocycloid(t, cfg); };
        case ParametricPreset::Spiral:
            return [cfg](double t) { return evaluate_spiral(t, cfg); };
        default:
            return [cfg](double t) { return evaluate_circle(t, cfg); };
    }
}

std::optional<std::string> ParametricCurveGenerator::generate_custom(
    const ParametricCurveConfig& config,
    std::vector<Point2D>& out_points
) {
    out_points.clear();
    out_points.reserve(config.sample_count);
    ExpressionParser parser;
    std::string error;

    if (config.custom_x_expr.empty() || config.custom_y_expr.empty() || config.sample_count < 3) {
        std::string error = "Empty expression or sample_count < 3";
        LOGT_ERROR(LogGeometry, "%s", error.c_str());
        return error;
    }

    if (!parser.validate(config.custom_x_expr)) {
        error = std::format("Invalid x expression: {} (error: {})", config.custom_x_expr, parser.last_error());
        LOGT_ERROR(LogGeometry, "%s", error.c_str());
        return error;
    }

    if (!parser.validate(config.custom_y_expr)) {
        error = std::format("Invalid y expression: {} (error: {})", config.custom_y_expr, parser.last_error());
        LOGT_ERROR(LogGeometry, "%s", error.c_str());
        return error;
    }

    out_points.reserve(config.sample_count);

    const double dt = (config.t_end - config.t_start) / config.sample_count; // avoids duplicating endpoint

    std::unordered_map<std::string, double> vars;
    vars["a"] = config.a;
    vars["b"] = config.b;
    vars["c"] = config.c;
    vars["pi"] = Math::PI;
    vars["e"] = Math::E;

    for (int i = 0; i < config.sample_count; ++i) {
        double t = config.t_start + i * dt;
        vars["t"] = t;

        auto x_val = parser.evaluate(config.custom_x_expr, vars);
        if (!x_val) {
            error = std::format("Failed to evaluate x expression at t={}: {}", t, parser.last_error());
            LOGT_ERROR(LogGeometry, "%s", error.c_str());
            return error;
        }

        auto y_val = parser.evaluate(config.custom_y_expr, vars);
        if (!y_val) {
            error = std::format("Failed to evaluate y expression at t={}: {}", t, parser.last_error());
            LOGT_ERROR(LogGeometry, "%s", error.c_str());
            return error;
        }

        Point2D pt;
        pt.p[0] = config.center.x + *x_val;
        pt.p[1] = config.center.y + *y_val;
        pt.id = i;
        pt.on_boundary = true;

        out_points.push_back(pt);
    }

    LOGT_DEBUG(LogGeometry, "Successfully generated %d custom parametric points", config.sample_count);

    return std::nullopt;
}

}