#ifndef FEM_MESH_GENERATOR_TYPES_H
#define FEM_MESH_GENERATOR_TYPES_H

#include "core/object/object.h"

// Some types that can be used by volumetric and planar mesh generators

namespace fem {

FEM_DECLARE_ENUM(TriangulationBackendType, CPU, GPU);

}


#endif // FEM_MESH_GENERATOR_TYPES_H