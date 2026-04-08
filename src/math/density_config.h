#ifndef FEM_DENSITY_CONFIG_H
#define FEM_DENSITY_CONFIG_H

#include "core/object/object.h"
#include "core/object/property.h"

namespace fem {

struct DensityConfig : public Struct {
    FEM_DECLARE_STRUCT(DensityConfig);
    FEM_DECLARE_PROPERTY_REGISTER(DensityConfig);

    bool enable = false;
    float global_h = 40.f;

    bool  use_boundary = true;
    float boundary_h_min = 14.f;
    float boundary_h_max = 40.f;
    float boundary_influence = 80.f;

    bool  use_radial = false;
    glm::dvec2 radial_center = glm::dvec2(300, 250);
    float radial_r_in = 40.f;
    float radial_r_out = 140.f;
    float radial_h_min = 10.f;
    float radial_h_max = 40.f;

    uint32_t max_steiner = 600;
    float L_over_h_threshold = 1.25f;
};



}

#endif // FEM_DENSITY_CONFIG_H