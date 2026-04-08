#include "planar_math_entity.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "math/pde/pde_component.h"

namespace fem {

FEM_DEFINE_OBJECT(PlanarMathEntity, MathEntity);
FEM_BEGIN_PROPERTY_REGISTER(PlanarMathEntity)
{

}
FEM_END_PROPERTY_REGISTER(PlanarMathEntity);

void PlanarMathEntity::init() {
    MathEntity::init();

    // Prevent creating components after deserialization.
    if (components_.empty()) {
        create_component<PDEComponent>();
        create_component<PlanarMeshComponent>();
    }
}

MeshComponent* PlanarMathEntity::add_mesh() {
    return create_component<PlanarMeshComponent>();
}

}