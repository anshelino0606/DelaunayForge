#ifndef FEM_PDE_PRESETS_H
#define FEM_PDE_PRESETS_H

#include "pde_preset.h"
#include "parameters/pde_coefficients.h"
#include "parameters/pde_constants.h"
#include "parameters/pde_fractional_operator.h"

namespace fem {

FEM_DEFINE_PDE_PRESET_CONFIG(PDEPreset_Laplace, PDEPresetFlag::NoRHS);

FEM_DECLARE_DEFAULT_STATIONARY_PDE_PRESET(
    PDEPreset_Laplace,
    Diffusivity
);

FEM_DEFINE_PDE_PRESET_CONFIG(PDEPreset_Poisson, PDEPresetFlag::HasExactSolution);

FEM_DECLARE_DEFAULT_STATIONARY_PDE_PRESET(
    PDEPreset_Poisson,
    Diffusivity
);

FEM_DECLARE_DEFAULT_STATIONARY_PDE_PRESET(
    PDEPreset_Reaction,
    Diffusivity,
    Reaction
);

FEM_DECLARE_DEFAULT_STATIONARY_PDE_PRESET(
    PDEPreset_Fractional,
    FractionalOperator
);

FEM_DECLARE_DEFAULT_TRANSIENT_PRESET_1IC(
    PDEPreset_Heat,
    Temperature,
    Diffusivity,
    Reaction
);

FEM_DEFINE_PDE_PRESET_CONFIG(PDEPreset_Wave, PDEPresetFlag::NoRHS);

FEM_DECLARE_DEFAULT_TRANSIENT_PRESET_2IC(
    PDEPreset_Wave,
    Displacement,
    Velocity,
    Diffusivity,
    Reaction
);

}

#endif // FEM_PDE_PRESETS_H