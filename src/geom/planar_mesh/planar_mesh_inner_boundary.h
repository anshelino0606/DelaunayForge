#ifndef FEM_PLANAR_MESH_INNER_BOUNDARY_H
#define FEM_PLANAR_MESH_INNER_BOUNDARY_H

#include "planar_mesh_boundary_base.h"

namespace fem {

class PlanarMeshInnerBoundary : public PlanarMeshBoundaryBase {
public:
    FEM_DECLARE_OBJECT(PlanarMeshInnerBoundary);
    FEM_DECLARE_PROPERTY_REGISTER(PlanarMeshInnerBoundary);

    PlanarMeshInnerBoundary();

private:
    void on_input_type_changed();
};

}

#endif // FEM_PLANAR_MESH_INNER_BOUNDARY_H