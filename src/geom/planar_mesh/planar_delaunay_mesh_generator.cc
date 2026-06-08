#include "planar_delaunay_mesh_generator.h"
#include "planar_mesh_component.h"
#include "planar_mesh_outer_boundary.h"
#include "planar_mesh_inner_boundary.h"

namespace fem {

FEM_DEFINE_OBJECT(PlanarDelaunayMeshGenerator, PlanarMeshGenerator);

FEM_BEGIN_PROPERTY_REGISTER(PlanarDelaunayMeshGenerator)
{
    FEM_REGISTER_PROPERTY(
        PlanarDelaunayMeshGenerator, 
        backend_type_,
        ON_VALUE_CHANGED(PlanarDelaunayMeshGenerator, switch_backend)
    );
}
FEM_END_PROPERTY_REGISTER(PlanarDelaunayMeshGenerator);

PlanarDelaunayMeshGenerator::PlanarDelaunayMeshGenerator() {
    backend_ = std::make_unique<CPUDelaunayBackend>();
    backend_type_ = TriangulationBackendType::CPU;
}

void PlanarDelaunayMeshGenerator::switch_backend() {
    switch (backend_type_) {
    case TriangulationBackendType::CPU: {
        backend_ = std::make_unique<CPUDelaunayBackend>();
        break;
    }
    case TriangulationBackendType::GPU: {
#if 0   // Not implemented yet
        backend_ = std::make_unique<GPUDelaunayBackend>(
            "placeholder",
            GPUDelaunayTriangulator::Mode::FULL_GPU
        );
#else
        backend_ = std::make_unique<CPUDelaunayBackend>();
        backend_type_ = TriangulationBackendType::CPU;
#endif
        break;
    }
    }
}

void PlanarDelaunayMeshGenerator::triangulate(PlanarMeshComponent* mesh_component) {
    const DensityConfig& density_config = mesh_component->density_config();

    TriangulationRequest request;
    request.density = mesh_component->build_density_function();

    request.config.min_angle_threshold = 0.0f;
    request.config.enable_lloyd_smoothing = false;
    request.config.lloyd_iterations = 3;
    request.config.enable_edge_flipping = false;
    request.config.enable_sizing_refinement = density_config.enable;
    request.config.refine_sizing_max_steiner = std::max(1u, density_config.max_steiner);
    request.config.density_refine_threshold  = density_config.L_over_h_threshold;
    
    std::vector<Point2D> points_storage;
    points_storage = mesh_component->outer_boundary()->points();

    for (size_t i = 0; i != points_storage.size(); ++i) {
        points_storage[i].id = i;
    }

    std::vector<std::vector<Point2D>> loops_storage;

    if (mesh_component->outer_boundary()->input_type() != BoundaryInputType::Polygon) {
        loops_storage.push_back(points_storage);
    }
    
    for (PlanarMeshInnerBoundary* inner_boundary : mesh_component->inner_boundaries()) {
        std::vector<Point2D>& loop = loops_storage.emplace_back();
        const std::vector<Point2D>& loop_points = inner_boundary->points();

        loop.reserve(loop_points.size());
        points_storage.reserve(points_storage.size() + loop_points.size());

        for (Point2D point : loop_points) {
            point.id = points_storage.size();
            point.on_boundary = true;
            points_storage.push_back(point);
            loop.push_back(point);
        }
    }

    request.points = &points_storage;
    request.boundary_loops = loops_storage.empty() ? nullptr : &loops_storage;

    DelaunayTriangulationResult triangulation_result = backend_->triangulate(request);
    mesh_component->update_triangulation(triangulation_result);
}

void PlanarDelaunayMeshGenerator::deserialize(Archive& archive) {
    PlanarMeshGenerator::deserialize(archive);
    switch_backend();
}

}