#ifndef FEM_SMOOTH_STROKE_CONFIG_H
#define FEM_SMOOTH_STROKE_CONFIG_H

#include "core/object/object.h"
#include "core/object/property.h"

namespace fem {

struct SmoothStrokeConfig : public Struct {
    FEM_DECLARE_STRUCT(SmoothStrokeConfig)
    FEM_DECLARE_PROPERTY_REGISTER(SmoothStrokeConfig);

    int    boundary_sample_count = 128;
    int    catmull_per_seg       = 16;
    double close_threshold_px    = 15.0;
    double min_dist_px           = 5.0;
    double screen_step_px        = 2.0;
};

}

#endif // FEM_SMOOTH_STROKE_CONFIG_H