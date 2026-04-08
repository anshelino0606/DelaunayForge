#include "pde_constants.h"

namespace fem {

FEM_DEFINE_PDE_SCALAR_CONSTANT(
    Temperature,
    Format("%.3g"),
    DragSpeed(0.1f),
    DisplayName("Initial T")
);

FEM_DEFINE_PDE_SCALAR_CONSTANT(
    Displacement,
    Format("%.3g"),
    DragSpeed(0.1f),
    DisplayName("Initial u")
);

FEM_DEFINE_PDE_SCALAR_CONSTANT(
    Velocity,
    Format("%.3g"),
    DragSpeed(0.1f),
    DisplayName("Initial v")
);

}