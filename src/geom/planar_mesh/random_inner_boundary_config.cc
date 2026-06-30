#include "random_inner_boundary_config.h"
#include "math/math_.h"

namespace fem {

FEM_DEFINE_ENUM(RandomInnerBoundarySourceType);
FEM_DEFINE_STRUCT(RandomInnerBoundaryConfig);

FEM_BEGIN_PROPERTY_REGISTER(RandomInnerBoundaryConfig)
{
    FEM_REGISTER_PROPERTY(
        RandomInnerBoundaryConfig,
        source_type,
        DisplayName("Template Type"),
        ON_VALUE_CHANGED(RandomInnerBoundaryConfig, on_source_type_changed)
    );

    FEM_REGISTER_PROPERTY(
        RandomInnerBoundaryConfig,
        count,
        ClampMin(1),
        ClampMax(1000)
    );

    FEM_REGISTER_PROPERTY(
        RandomInnerBoundaryConfig,
        seed,
        ClampMin(0),
        ClampMax(1000000000)
    );

    FEM_REGISTER_PROPERTY(
        RandomInnerBoundaryConfig,
        max_attempts_per_boundary,
        DisplayName("Attempts Per Boundary"),
        ClampMin(1),
        ClampMax(10000)
    );

    FEM_REGISTER_PROPERTY(
        RandomInnerBoundaryConfig,
        min_clearance,
        DisplayName("Min Clearance"),
        ClampMin(0.0),
        ClampMax(500.0)
    );

    FEM_REGISTER_PROPERTY(RandomInnerBoundaryConfig, replace_existing);

    FEM_REGISTER_PROPERTY(
        RandomInnerBoundaryConfig,
        parametric_template,
        NoTypeHeader(),
        SHOW_FOR_ENUM(source_type, RandomInnerBoundarySourceType::Parametric)
    );

    FEM_REGISTER_PROPERTY(
        RandomInnerBoundaryConfig,
        fractal_template,
        NoTypeHeader(),
        SHOW_FOR_ENUM(source_type, RandomInnerBoundarySourceType::Fractal)
    );
}
FEM_END_PROPERTY_REGISTER(RandomInnerBoundaryConfig)

RandomInnerBoundaryConfig::RandomInnerBoundaryConfig() {
    on_source_type_changed();
}

void RandomInnerBoundaryConfig::on_source_type_changed() {
    if (source_type == RandomInnerBoundarySourceType::Parametric) {
        parametric_template.preset = ParametricPreset::Circle;
        parametric_template.radius = 18.0;
        parametric_template.sample_count = 48;
        parametric_template.t_start = 0.0;
        parametric_template.t_end = math::TWO_PI;
    } else {
        fractal_template.preset = FractalPreset::KochSnowflake;
        fractal_template.iterations = 1;
        fractal_template.sample_count = 96;
        fractal_template.snowflake_side_length = 28.0;
        fractal_template.size = 18.0;
        fractal_template.seed = 1;
        fractal_template.roughness = 0.35;
    }
}

} // namespace fem