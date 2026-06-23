#ifndef FEM_VOLUMETRIC_MESH_COMPONENT_H
#define FEM_VOLUMETRIC_MESH_COMPONENT_H

#include "geom/mesh/mesh_component.h"

namespace fem {

// TODO
class VolumetricMeshComponent : public MeshComponent {
public:
    FEM_DECLARE_OBJECT(VolumetricMeshComponent);
    FEM_DECLARE_PROPERTY_REGISTER(VolumetricMeshComponent);
};

}

#endif // FEM_VOLUMETRIC_MESH_COMPONENT_H