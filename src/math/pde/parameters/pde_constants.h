#ifndef FEM_PDE_CONSTANTS_H
#define FEM_PDE_CONSTANTS_H

#include "pde_parameter.h"

namespace fem {

FEM_DECLARE_PDE_SCALAR_CONSTANT(Temperature, 20.0);
FEM_DECLARE_PDE_SCALAR_CONSTANT(Displacement, 0.0);
FEM_DECLARE_PDE_SCALAR_CONSTANT(Velocity, 0.0);

}

#endif // FEM_PDE_CONSTANTS_H