#include "planar_mesh_generator.h"
#include "planar_delaunay_mesh_generator.h"

namespace fem {

FEM_DEFINE_OBJECT(PlanarMeshGenerator, Object, AbstractClass(), BaseClass());

FEM_BEGIN_PROPERTY_REGISTER(PlanarMeshGenerator)
{

}
FEM_END_PROPERTY_REGISTER(PlanarMeshGenerator);

PlanarMeshGenerator* PlanarMeshGenerator::default_generator() {
    return create_object<PlanarDelaunayMeshGenerator>();
}

}