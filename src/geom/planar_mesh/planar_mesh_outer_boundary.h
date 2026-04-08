#ifndef FEM_PLANAR_MESH_OUTER_BOUNDARY_H
#define FEM_PLANAR_MESH_OUTER_BOUNDARY_H

#include "planar_mesh_boundary_base.h"

namespace fem {

class PlanarMeshComponent;

class PlanarMeshOuterBoundary : public PlanarMeshBoundaryBase {
public:
    FEM_DECLARE_OBJECT(PlanarMeshOuterBoundary);
    FEM_DECLARE_PROPERTY_REGISTER(PlanarMeshOuterBoundary);

    // Think about params?
    void generate_random_points();
    void generate_grid();

    void generate_fractal_domain();
};

}

#endif // FEM_PLANAR_MESH_OUTER_BOUNDARY_H