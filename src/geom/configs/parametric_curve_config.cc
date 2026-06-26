#include "parametric_curve_config.h"
#include "math/math_.h"

namespace fem {

FEM_DEFINE_ENUM(ParametricPreset);
FEM_DEFINE_ENUM(ParametricCustomPresetExample);

FEM_DEFINE_STRUCT(ParametricCurveConfig);

FEM_BEGIN_PROPERTY_REGISTER(ParametricCurveConfig)
{
    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        preset, 
        ON_VALUE_CHANGED(ParametricCurveConfig, on_preset_changed)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        custom_preset_example,
        DisplayName("Example"),
        ON_VALUE_CHANGED(ParametricCurveConfig, on_custom_preset_example_changed),
        SHOW_FOR_ENUM(preset, ParametricPreset::Custom)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        radius,
        DisplayName("Radius"),
        ClampMin(10.0),
        ClampMax(300.0),
        SHOW_FOR_ENUM(preset, ParametricPreset::Circle)
    );
    
    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        width,
        DisplayName("Width (a)"),
        ClampMin(10.0f),
        ClampMax(300.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Ellipse)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        height,
        DisplayName("Height (b)"),
        ClampMin(10.0f),
        ClampMax(300.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Ellipse)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        size,
        DisplayName("Size (a)"),
        ClampMin(10.0f),
        ClampMax(200.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Cardioid, ParametricPreset::Lemniscate)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        outer_radius,
        DisplayName("Outer Radius (a)"),
        ClampMin(20.0f),
        ClampMax(200.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Epicycloid, ParametricPreset::Hypocycloid)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        inner_radius,
        DisplayName("Inner Radius (b)"),
        ClampMin(5.0f),
        ClampMax(100.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Epicycloid, ParametricPreset::Hypocycloid)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        start_radius,
        DisplayName("Start radius (a)"),
        ClampMin(0.0f),
        ClampMax(100.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Spiral)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        growth_rate,
        DisplayName("Growth rate (b)"),
        ClampMin(0.5f),
        ClampMax(20.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Spiral)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        custom_x_expr,
        DisplayName("x(t)"),
        SHOW_FOR_ENUM(preset, ParametricPreset::Custom)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        custom_y_expr,
        DisplayName("y(t)"),
        SHOW_FOR_ENUM(preset, ParametricPreset::Custom)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig,
        a,
        DisplayName("Parameter a"),
        ClampMin(1.0f),
        ClampMax(300.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Custom)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig,
        b,
        DisplayName("Parameter b"),
        ClampMin(1.0f),
        ClampMax(300.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Custom)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig,
        c,
        DisplayName("Parameter c"),
        ClampMin(0.1f),
        ClampMax(10.0f),
        SHOW_FOR_ENUM(preset, ParametricPreset::Custom)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        t_start,
        DisplayName("t start"),
        ClampMin(-4.0f * math::F_PI),
        ClampMax(4.0f * math::F_PI),
        Format("%.3f")
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        t_end,
        DisplayName("t end"),
        ClampMin(-4.0f * math::F_PI),
        ClampMax(8.0f * math::F_PI),
        Format("%.3f")
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        sample_count,
        ClampMin(16),
        ClampMax(512)
    );

    FEM_REGISTER_PROPERTY(
        ParametricCurveConfig, 
        center,
        ClampMin(0),
        ClampMax(5000.0f)
    );
}
FEM_END_PROPERTY_REGISTER(ParametricCurveConfig)

void ParametricCurveConfig::on_preset_changed() {
    switch (preset) {
        case ParametricPreset::Circle:
            radius = 100.0; t_start = 0.0; t_end = 2*math::PI;
            break;
        case ParametricPreset::Ellipse:
            width = 150.0; height = 80.0; t_start = 0.0; t_end = 2*math::PI;
            break;
        case ParametricPreset::Cardioid:
            size = 80.0; t_start = 0.0; t_end = 2*math::PI;
            break;
        case ParametricPreset::Lemniscate:
            size = 100.0; t_start = -math::PI; t_end = math::PI;
            break;
        case ParametricPreset::Epicycloid:
            outer_radius = 100.0; inner_radius = 30.0; t_start = 0.0; t_end = 2*math::PI;
            break;
        case ParametricPreset::Hypocycloid:
            outer_radius = 120.0; inner_radius = 40.0; t_start = 0.0; t_end = 2*math::PI;
            break;
        case ParametricPreset::Spiral:
            start_radius = 20.0; inner_radius = 5.0; t_start = 0.0; t_end = 6*math::PI;
            break;
        case ParametricPreset::Custom:
            custom_x_expr = "a*cos(t)";
            custom_y_expr = "a*sin(t)";
            a = 100.0;
            b = 100.0;
            c = 1.0;
            custom_preset_example = ParametricCustomPresetExample::Circle;
            break;
    }
}

void ParametricCurveConfig::on_custom_preset_example_changed() {
    switch (custom_preset_example) {
        case ParametricCustomPresetExample::Circle:
            custom_x_expr = "a*cos(t)";
            custom_y_expr = "a*sin(t)";
            break;
        case ParametricCustomPresetExample::Ellipse:
            custom_x_expr = "a*cos(t)";
            custom_y_expr = "b*sin(t)";
            break;
        case ParametricCustomPresetExample::Figure8:
            custom_x_expr = "a*sin(t)";
            custom_y_expr = "a*sin(t)*cos(t)";
            break;
        case ParametricCustomPresetExample::Superellipse:
            custom_x_expr = "a*pow(abs(cos(t)), 2/3)*sign(cos(t))";
            custom_y_expr = "b*pow(abs(sin(t)), 2/3)*sign(sin(t))";
            break;
        case ParametricCustomPresetExample::Astroid:
            custom_x_expr = "a*pow(cos(t), 3)";
            custom_y_expr = "a*pow(sin(t), 3)";
            break;
        case ParametricCustomPresetExample::Deltoid:
            custom_x_expr = "2*a*cos(t) + a*cos(2*t)";
            custom_y_expr = "2*a*sin(t) - a*sin(2*t)";
            break;
        case ParametricCustomPresetExample::Lissajous:
            custom_x_expr = "a*sin(3*t)";
            custom_y_expr = "b*sin(2*t)";
            break;
        case ParametricCustomPresetExample::Butterfly:
            custom_x_expr = "a*sin(t)*(exp(cos(t)) - 2*cos(4*t) - pow(sin(t/12), 5))";
            custom_y_expr = "a*cos(t)*(exp(cos(t)) - 2*cos(4*t) - pow(sin(t/12), 5))";
            t_start = 0.0;
            t_end = 12.0 * 3.14159265358979323846;
            break;
    }
}

}