#include "delaunay_mesh_generator.h"
#include "planar_mesh/planar_mesh_component.h"

#include "delaunay_gpu.h"
#include "math/curve.h"
#include "log_categories.h"
#include <algorithm>
#include <utility> 

namespace fem {

DelaunayMeshGenerator::~DelaunayMeshGenerator() = default;

DelaunayMeshGenerator::DelaunayMeshGenerator() {
    backend_ = std::make_shared<CPUDelaunayBackend>();
    backend_type_ = fem::TriBackendType::CPU;
}

void DelaunayMeshGenerator::init() {

}

void DelaunayMeshGenerator::shutdown() {

}

// TEMP
std::vector<glm::vec3> DelaunayMeshGenerator::generateMesh() {
    return {};
}

void DelaunayMeshGenerator::generate_mesh(const fem::DelaunayMeshGeneratorConfig& config, fem::PlanarMeshComponent* mesh) {
    if (!backend_ || config.backend_type != backend_type_) {
        switch (config.backend_type) {
            case fem::TriBackendType::CPU:
                backend_ = std::make_shared<CPUDelaunayBackend>();
                break;
            case fem::TriBackendType::GPU:
#if 0
                backend_ = std::make_shared<GPUDelaunayBackend>(
                    "placeholder",
                    GPUDelaunayTriangulator::Mode::FULL_GPU
                );
#endif
                LOGT_WARN(LogGeometry, "GPU triangulation requested but not implemented yes. Falling back to CPU backend");
                backend_ = std::make_shared<CPUDelaunayBackend>();
                break;
        }

        backend_type_ = config.backend_type;
    }

    switch (config.mode) {
        case fem::DelaunayMeshGeneratorMode::POINTS_ONLY:
            if (mesh->user_points().empty()) {
                return;
            }
            break;

        case fem::DelaunayMeshGeneratorMode::WITH_BOUNDARY:
            if (mesh->user_points().empty() &&
                mesh->boundary_points().empty() &&
                mesh->boundary_loops().empty())
            {
                return;
            }
            break;

        case fem::DelaunayMeshGeneratorMode::POLYGON_CLIP:
            if (mesh->polygon_points().size() < 3) {
                return;
            }
            break;
    }

    TriangulationRequest req;
    req.config  = config;
    req.density = density_fn_;;

    std::vector<Point2D> points_storage;
    points_storage = mesh->user_points();

    for (int i = 0; i < (int)points_storage.size(); ++i) {
        points_storage[i].id = i;
        // keep on_boundary as-is for user points
    }


    std::vector<std::vector<Point2D>> loops_storage;

    if (config.mode == fem::DelaunayMeshGeneratorMode::WITH_BOUNDARY) {
        if (!mesh->boundary_loops().empty()) {
            loops_storage.reserve(mesh->boundary_loops().size());
            for (const auto& loop : mesh->boundary_loops()) {
                const std::vector<Point2D>& loop_points = loop.points;

                std::vector<Point2D> L;
                L.reserve(loop_points.size());
                for (auto p : loop_points) {
                    p.on_boundary = true;
                    p.id = (int)points_storage.size();
                    points_storage.push_back(p);
                    L.push_back(p); // ids now match points_storage
                }
                loops_storage.push_back(std::move(L));
            }
        } else if (!mesh->boundary_points().empty()) {
            std::vector<Point2D> L;
            L.reserve(mesh->boundary_points().size());
            for (auto p : mesh->boundary_points()) {
                p.on_boundary = true;
                p.id = (int)points_storage.size();
                points_storage.push_back(p);
                L.push_back(p);
            }
            loops_storage.push_back(std::move(L));
        }

        req.boundary_loops = loops_storage.empty() ? nullptr : &loops_storage;
    }

    if (config.mode == fem::DelaunayMeshGeneratorMode::POLYGON_CLIP) {
        req.clip_polygon = &mesh->polygon_points();
    }


    
    req.points = &points_storage;

    const auto result = backend_->triangulate(req);
    mesh->update_triangulation(result);

    // if (mesh->triangulation_result().points.empty()) {
        // indices_cache.clear();
        // return;
    // }
}

// TEMP
const std::vector<unsigned int>& DelaunayMeshGenerator::getIndices() const {
    static std::vector<unsigned int> temp; 
    return temp;
}

}