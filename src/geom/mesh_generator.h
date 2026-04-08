//
// Created by anmode on 26.08.2024.
//

#ifndef FEM_ENGINE_MESH_GENERATOR_H
#define FEM_ENGINE_MESH_GENERATOR_H

#include "glm/glm.hpp"
#include <vector>

namespace fem {

class MeshGeneratorStrategy {
public:
    virtual ~MeshGeneratorStrategy() = default;
    virtual std::vector<glm::vec3> generateMesh() = 0;
    virtual const std::vector<unsigned int>& getIndices() const = 0;
};

}

#endif //FEM_ENGINE_MESH_GENERATOR_H
