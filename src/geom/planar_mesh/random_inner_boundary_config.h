#ifndef FEM_RANDOM_INNER_BOUNDARY_CONFIG_H
#define FEM_RANDOM_INNER_BOUNDARY_CONFIG_H

#include "core/object/object.h"
#include "core/object/property.h"
#include "geom/configs/fractal_domain_config.h"
#include "geom/configs/parametric_curve_config.h"

namespace fem {

FEM_DECLARE_ENUM(RandomInnerBoundarySourceType, Parametric, Fractal);

struct RandomInnerBoundaryConfig : public Struct {
    FEM_DECLARE_STRUCT(RandomInnerBoundaryConfig);
    FEM_DECLARE_PROPERTY_REGISTER(RandomInnerBoundaryConfig);

    RandomInnerBoundarySourceType source_type = RandomInnerBoundarySourceType::Parametric;
    int count = 8;
    uint32_t seed = 1;
    int max_attempts_per_boundary = 64;
    double min_clearance = 8.0;
    bool replace_existing = false;

    ParametricCurveConfig parametric_template;
    FractalDomainConfig fractal_template;

    RandomInnerBoundaryConfig();

private:
    void on_source_type_changed();
};

} // namespace fem

#endif // FEM_RANDOM_INNER_BOUNDARY_CONFIG_H