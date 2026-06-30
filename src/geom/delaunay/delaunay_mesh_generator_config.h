#ifndef FEM_DELAUNAY_MESH_GENERATOR_CONFIG_H
#define FEM_DELAUNAY_MESH_GENERATOR_CONFIG_H

#include "delaunay_types.h"

namespace fem {

enum class DelaunayMeshGeneratorMode { 
    POINTS_ONLY, 
    WITH_BOUNDARY, 
    POLYGON_CLIP 
};

enum class TriBackendType {
    CPU,
    GPU,
};

struct DelaunayMeshGeneratorConfig : public DelaunayTriangulationConfig {
    TriBackendType backend_type = TriBackendType::GPU;
    DelaunayMeshGeneratorMode mode = DelaunayMeshGeneratorMode::POINTS_ONLY;
};

}

#endif // FEM_DELAUNAY_MESH_GENERATOR_CONFIG_H
