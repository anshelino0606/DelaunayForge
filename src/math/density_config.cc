#include "density_config.h"

namespace fem {

#define SHOW_WHEN_ENABLED() \
    SHOW_WHEN_MEMBER(DensityConfig, enable, val == true)

#define SHOW_WHEN_BOUNDARY_ENABLED()                                                \
    EditConditionMember([](const void* owner) {                                     \
        const DensityConfig& config = *static_cast<const DensityConfig*>(owner);    \
        return config.enable && config.use_boundary;                                \
    })

#define SHOW_WHEN_RADIAL_HOTSPOT_ENABLED()                                          \
    EditConditionMember([](const void* owner) {                                     \
        const DensityConfig& config = *static_cast<const DensityConfig*>(owner);    \
        return config.enable && config.use_radial;                                  \
    })

FEM_DEFINE_STRUCT(DensityConfig, DisplayName("Density (sizing field)"));
FEM_BEGIN_PROPERTY_REGISTER(DensityConfig)
{
    FEM_REGISTER_PROPERTY(DensityConfig, enable, DisplayName("Enable density refinement"));
    
    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        global_h, 
        DisplayName("Global h"),
        SHOW_WHEN_ENABLED(),
        ClampMin(1.05f),
        ClampMax(100.0f)
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        max_steiner, 
        DisplayName("Max Steiner"),
        SHOW_WHEN_ENABLED(),
        ClampMin(0),
        ClampMax(5000)
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        L_over_h_threshold,
        DisplayName("Refine when L/h >"),
        SHOW_WHEN_ENABLED(),
        ClampMin(1.05f),
        ClampMax(2.0f),
        Format("%.2f")
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        use_boundary, 
        DisplayName("Denser near boundary"),
        SHOW_WHEN_ENABLED()
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        boundary_h_min,
        DisplayName("Boundary h_min"),
        SHOW_WHEN_BOUNDARY_ENABLED(),
        ClampMin(3.0f),
        ClampMax(60.0f),
        Format("%.1f")
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        boundary_h_max,
        DisplayName("Boundary h_max"),
        SHOW_WHEN_BOUNDARY_ENABLED(),
        ClampMin(5.0f),
        ClampMax(60.0f),
        Format("%.1f")
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        boundary_influence,
        SHOW_WHEN_BOUNDARY_ENABLED(),
        ClampMin(5.0f),
        ClampMax(200.0f),
        Format("%.1f")
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        use_radial,
        DisplayName("Radial hotspot"),
        SHOW_WHEN_ENABLED()
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        radial_center,
        DisplayName("Hotspot Center"),
        SHOW_WHEN_RADIAL_HOTSPOT_ENABLED(),
        ClampMin(0.0f),
        ClampMax(1000.0f) // ???  
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        radial_r_in,
        DisplayName("Inner R"),
        SHOW_WHEN_RADIAL_HOTSPOT_ENABLED(),
        ClampMin(5.0f),
        ClampMax(300.0f),
        Format("%.1f")
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        radial_r_out,
        DisplayName("Outer R"),
        SHOW_WHEN_RADIAL_HOTSPOT_ENABLED(),
        ClampMin(10.0f),
        ClampMax(400.0f),
        Format("%.1f")
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        radial_h_min,
        DisplayName("Hotspot h_min"),
        SHOW_WHEN_RADIAL_HOTSPOT_ENABLED(),
        ClampMin(3.0f),
        ClampMax(60.0f),
        Format("%.1f")
    );

    FEM_REGISTER_PROPERTY(
        DensityConfig, 
        radial_h_max,
        DisplayName("Hotspot h_max"),
        SHOW_WHEN_RADIAL_HOTSPOT_ENABLED(),
        ClampMin(5.0f),
        ClampMax(140.0f),
        Format("%.1f")
    );
}
FEM_END_PROPERTY_REGISTER(DensityConfig);

}