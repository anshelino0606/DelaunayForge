#ifndef FEM_MESH_COMPONENT_H
#define FEM_MESH_COMPONENT_H

#include "core/entity/component.h"

namespace fem {

class MeshComponent : public Component {
public:
    FEM_DECLARE_OBJECT(MeshComponent);
    FEM_DECLARE_PROPERTY_REGISTER(MeshComponent);

    
};

}

#endif // FEM_MESH_COMPONENT_H