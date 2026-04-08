#ifndef FEM_PLANAR_DELAUNAY_MESH_GENERATOR_H
#define FEM_PLANAR_DELAUNAY_MESH_GENERATOR_H

#include "planar_mesh_generator.h"
#include "geom/triangulation_backend.h"

namespace fem {

class PlanarDelaunayMeshGenerator : public PlanarMeshGenerator {
public:
    FEM_DECLARE_OBJECT(PlanarDelaunayMeshGenerator);
    FEM_DECLARE_PROPERTY_REGISTER(PlanarDelayunayMeshGenerator);

    PlanarDelaunayMeshGenerator();

    void switch_backend();

    virtual void triangulate(PlanarMeshComponent* mesh_component) override;
    virtual void deserialize(Archive& archive) override;

protected:
    std::unique_ptr<ITriangulationBackend> backend_ = nullptr;
};

}

#endif // FEM_PLANAR_DELAUNAY_MESH_GENERATOR_H