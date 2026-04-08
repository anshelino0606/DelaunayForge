#ifndef FEM_PDE_COEFFICIENTS_H
#define FEM_PDE_COEFFICIENTS_H

#include "pde_parameter.h"

namespace fem {

FEM_DECLARE_PDE_SCALAR_PARAMETER_DEFAULT_VALUE(Diffusivity, DifferentialEquation::a, 1);
FEM_DECLARE_PDE_SCALAR_PARAMETER(Reaction, DifferentialEquation::c);

}

#endif // FEM_PDE_COEFFICIENTS_H