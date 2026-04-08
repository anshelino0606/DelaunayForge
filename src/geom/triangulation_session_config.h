#ifndef FEM_TRIANGULATION_SESSION_CONFIG_H
#define FEM_TRIANGULATION_SESSION_CONFIG_H

#include "delaunay_mesh_generator_config.h"
#include "math/density_functions.h"
#include "math/density_config.h"

namespace fem {

class PlanarMeshComponent;
class VolumetricMeshComponent;

struct PlanarTriangulationSessionConfig {
    PlanarMeshComponent* mesh = nullptr;
    DelaunayMeshGeneratorConfig mesh_generator_config;
    DensityConfig density_config;
    std::shared_ptr<DensityFunction> density_function;
    
};

// TODO
struct VolumetricTriangulationSessionConfig {
    VolumetricMeshComponent* mesh = nullptr;
};

}


#endif // FEM_TRIANGULATION_SESSION_CONFIG_H