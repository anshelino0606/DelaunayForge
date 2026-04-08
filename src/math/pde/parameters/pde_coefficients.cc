#include "pde_coefficients.h"
#include "math/differential_equation.h"

using namespace fem::PDEParameters;

namespace fem {

FEM_DEFINE_PDE_SCALAR_PARAMETER(
    Diffusivity,
    DisplayName("a (diffusivity)"),
    DragSpeed(0.01f),
    ClampMin(0.0f),
    ClampMax(1e6f),
    Format("%.3g")
);

FEM_DEFINE_PDE_SCALAR_PARAMETER(
    Reaction, 
    DisplayName("c (reaction)"),
    DragSpeed(0.01f),
    ClampMin(0.0f),
    ClampMax(1e6f),
    Format("%.3g")
);

}
