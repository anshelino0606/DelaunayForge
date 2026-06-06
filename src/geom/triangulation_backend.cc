#include "triangulation_backend.h"
#include "geom/delaunay2d.h"

namespace fem {

// CPU BACKEND
DelaunayTriangulationResult CPUDelaunayBackend::triangulate(
    const TriangulationRequest& req)
{
    DelaunayTriangulationResult R;

    const bool have_pts   = (req.points && !req.points->empty());
    const bool have_loops = (req.boundary_loops && !req.boundary_loops->empty());
    const bool have_clip  = (req.clip_polygon  && !req.clip_polygon->empty());

    if (!have_pts && !have_loops && !have_clip) {
        return R;
    }

    static const std::vector<Point2D> empty_pts;
    const std::vector<Point2D>& pts = (req.points ? *req.points : empty_pts);

    DelaunayTriangulator tri(req.config);
    if (req.density) {
        tri.set_density_function(req.density);
    }

    if (have_loops) {
        const auto& loops = *req.boundary_loops;
        if (loops.size() == 1u && !have_clip) {
            R = tri.triangulate_with_boundary(*req.points, loops[0]);
        } else {
            R = tri.triangulate_with_boundaries(*req.points, loops);
        }
    } else if (have_clip) {
        R = tri.triangulate_with_boundary(*req.points, *req.clip_polygon);
    } else {
        R = tri.triangulate(*req.points);
    }
    
    return R;
}

// GPU BACKEND
GPUDelaunayBackend::GPUDelaunayBackend(
    const char* shader_dir,
    GPUDelaunayTriangulator::Mode mode)
{
    gpu_ = create_delaunay_triangulator(mode);
}

DelaunayTriangulationResult GPUDelaunayBackend::triangulate(
    const TriangulationRequest& req)
{
    DelaunayTriangulationResult R;

    if (!gpu_) {
        return R;
    }

    if (req.density) {
        gpu_->set_density_function(req.density);
    }

    const bool have_pts   = (req.points && !req.points->empty());
    const bool have_loops = (req.boundary_loops && !req.boundary_loops->empty());
    const bool have_clip  = (req.clip_polygon  && !req.clip_polygon->empty());

    if (!have_pts && !have_loops && !have_clip) {
        return R;
    }

    const auto& cfg = req.config;

    std::vector<Point2D> tmp_points;
    const std::vector<Point2D>* gpu_points = nullptr;

    if (have_pts) {
        gpu_points = req.points;
    } else if (have_loops) {
        const auto& loops = *req.boundary_loops;
        for (const auto& loop : loops) {
            tmp_points.insert(tmp_points.end(), loop.begin(), loop.end());
        }
        gpu_points = &tmp_points;
    }

    if (!gpu_points || gpu_points->empty()) {
        return R;
    }

    if (have_loops) {
        const auto& loops = *req.boundary_loops;

        if (loops.size() == 1u && !have_clip) {
            return gpu_->triangulate_with_boundary_gpu(*gpu_points, loops[0], cfg);
        }
        return gpu_->triangulate_with_boundaries_gpu(*gpu_points, loops, cfg);
    }

    if (have_clip) {
        return gpu_->triangulate_with_boundary_gpu(*gpu_points, *req.clip_polygon, cfg);
    }

    return gpu_->triangulate_full_gpu(*gpu_points, cfg);
}
}
