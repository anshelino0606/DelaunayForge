#include "mesh_component.h"
#include "math/boundary_condition.h"

namespace fem {

FEM_DEFINE_OBJECT(MeshComponent, Object);

FEM_BEGIN_PROPERTY_REGISTER(MeshComponent)
{

}
FEM_END_PROPERTY_REGISTER(MeshComponent)

MeshComponent::~MeshComponent() {
    for (BoundaryCondition* condition : boundary_conditions_) {
        destroy_object(condition);
    }
}

}