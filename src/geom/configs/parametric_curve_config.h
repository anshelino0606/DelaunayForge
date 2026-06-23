#ifndef FEM_PARAMETRIC_CURVE_CONFIG_H
#define FEM_PARAMETRIC_CURVE_CONFIG_H

#include "core/object/object.h"
#include "core/object/property.h"

namespace fem {

FEM_DECLARE_ENUM(
    ParametricPreset,
    Circle,
    Ellipse,
    Cardioid,
    Lemniscate,
    Epicycloid,
    Hypocycloid,
    Spiral,
    Custom
);

FEM_DECLARE_ENUM(
    ParametricCustomPresetExample,
    Circle,
    Ellipse,
    Figure8,
    Superellipse,
    Astroid,
    Deltoid,
    Lissajous,
    Butterfly
);

struct ParametricCurveConfig : public Struct {
    FEM_DECLARE_STRUCT(ParametricCurveConfig);
    FEM_DECLARE_PROPERTY_REGISTER(ParametricCurveConfig);

    ParametricPreset preset = ParametricPreset::Circle;
    ParametricCustomPresetExample custom_preset_example = ParametricCustomPresetExample::Circle;

    double radius = 1.0;
    double width = 1.0;
    double height = 1.0;
    double size = 1.0;
    double outer_radius = 1.0;
    double inner_radius = 1.0;
    double petal_length = 1.0;
    double petal_count = 1.0;
    double start_radius = 1.0;
    double growth_rate = 1.0;
    
    double t_start = 0.0;
    double t_end = 2.0 * 3.14159265358979323846;
    
    int sample_count = 128;
    
    glm::dvec2 center{450, 300};    // Values are temp
    
    std::string custom_x_expr;
    std::string custom_y_expr;

    // Parameters for evaluation
    double a = 100.0;
    double b = 100.0;
    double c = 1.0;

private:
    void on_preset_changed();
    void on_custom_preset_example_changed();
};

}

#endif //FEM_PARAMETRIC_CURVE_CONFIG_H