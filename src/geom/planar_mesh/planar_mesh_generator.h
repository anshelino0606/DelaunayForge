#ifndef FEM_PLANAR_MESH_GENERATOR_H
#define FEM_PLANAR_MESH_GENERATOR_H

#include "core/object/object.h"
#include "core/object/property.h"
#include "geom/mesh/mesh_generator_types.h"

namespace fem {

class PlanarMeshComponent;

class PlanarMeshGenerator : public Object {
public:
    FEM_DECLARE_OBJECT(PlanarMeshGenerator);
    FEM_DECLARE_PROPERTY_REGISTER(PlanarMeshGenerator);

    static PlanarMeshGenerator* default_generator();

    virtual void triangulate(PlanarMeshComponent* mesh_component) {}

protected:
    TriangulationBackendType backend_type_ = TriangulationBackendType::GPU;
};

}

#endif // FEM_PLANAR_MESH_GENERATOR_H