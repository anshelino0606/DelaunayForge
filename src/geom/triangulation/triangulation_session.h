#ifndef FEM_TRIANGULATION_SESSION_H
#define FEM_TRIANGULATION_SESSION_H

#include "geom/delaunay/delaunay_mesh_generator.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <utility>
#include <glm/glm.hpp>

namespace fem {

class TriangulationSession {
public:
    using BCType = fem::BCType;

    void init();
    void shutdown();

    std::shared_ptr<DelaunayMeshGenerator> mesh_generator() const { return mesh_generator_; }

    void clear_all();

private:
    std::shared_ptr<DelaunayMeshGenerator> mesh_generator_;

    void subscribe_to_events();
};

}

#endif // FEM_TRIANGULATION_SESSION_H
