#include "smooth_stroke_config.h"

namespace fem {

FEM_DEFINE_STRUCT(SmoothStrokeConfig);
FEM_BEGIN_PROPERTY_REGISTER(SmoothStrokeConfig)
{
    FEM_REGISTER_PROPERTY(
        SmoothStrokeConfig, 
        boundary_sample_count,
        ClampMin(6),
        ClampMax(1024)
    );

    FEM_REGISTER_PROPERTY(SmoothStrokeConfig, catmull_per_seg, NoUI());
    FEM_REGISTER_PROPERTY(SmoothStrokeConfig, close_threshold_px, NoUI());
    FEM_REGISTER_PROPERTY(SmoothStrokeConfig, min_dist_px, NoUI());
    FEM_REGISTER_PROPERTY(SmoothStrokeConfig, screen_step_px, NoUI());
}
FEM_END_PROPERTY_REGISTER(SmoothStrokeConfig)

}