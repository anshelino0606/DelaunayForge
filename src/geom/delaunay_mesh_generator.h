#ifndef DELAUNAY_MESH_GENERATOR_H
#define DELAUNAY_MESH_GENERATOR_H

#include "mesh_generator.h"
#include "triangulation_backend.h"
#include "delaunay_mesh_generator_config.h"
#include <memory>
#include <vector>

namespace fem {

class PlanarMeshComponent;

class DelaunayMeshGenerator : public MeshGeneratorStrategy {
private:
    std::shared_ptr<ITriangulationBackend> backend_;
    std::shared_ptr<DensityFunction> density_fn_;
    fem::TriBackendType backend_type_ = fem::TriBackendType::GPU;

public:
    DelaunayMeshGenerator();
    ~DelaunayMeshGenerator();

    void init();
    void shutdown();

    void set_density_function(std::shared_ptr<DensityFunction> f) { density_fn_ = std::move(f); }

    std::vector<glm::vec3> generateMesh() override;
    const std::vector<unsigned int>& getIndices() const override;

    // JeFFlidan: No utilizing interface for now. I will think about MeshGeneratorStrategy interface in the future
    void generate_mesh(const DelaunayMeshGeneratorConfig& config, PlanarMeshComponent* mesh);
};

}

#endif // DELAUNAY_MESH_GENERATOR_H
