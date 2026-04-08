#ifndef TRIANGULATION_BACKEND_H
#define TRIANGULATION_BACKEND_H
#include <memory>
#include <vector>
#include "geom/delaunay2d.h"

#ifdef USE_BGFX
#include "delaunay_gpu.h"
#include "delaunay_compute.h"
#endif // USE_BGFX


namespace fem {

class DensityFunction;

struct TriangulationRequest {
    const std::vector<Point2D>* points = nullptr;
    const std::vector<std::vector<Point2D>>* boundary_loops = nullptr;
    const std::vector<Point2D>* clip_polygon = nullptr;
    std::shared_ptr<DensityFunction> density;
    DelaunayTriangulationConfig     config;
};

/// Strategy interface for "something that can build a Delaunay mesh".
class ITriangulationBackend {
public:
    virtual ~ITriangulationBackend() = default;
    virtual DelaunayTriangulationResult triangulate(
        const TriangulationRequest& request) = 0;
    virtual const char* name() const noexcept = 0;
};

class CPUDelaunayBackend final : public ITriangulationBackend {
public:
    ~CPUDelaunayBackend() override = default;
    
    DelaunayTriangulationResult triangulate(
        const TriangulationRequest& req) override;
    
    const char* name() const noexcept override { return "CPU Delaunay"; }
};

// omg idk but w/o this it breaks the macos gcc, we should do smth maybe
#ifdef USE_BGFX
class GPUDelaunayBackend final : public ITriangulationBackend {
public:
    explicit GPUDelaunayBackend(const char* shader_dir = nullptr,
                                GPUDelaunayTriangulator::Mode mode =
                                    GPUDelaunayTriangulator::Mode::HYBRID);
    
    DelaunayTriangulationResult triangulate(
        const TriangulationRequest& request) override;


    
    const char* name() const noexcept override { return "GPU Delaunay"; }

private:
    std::unique_ptr<GPUDelaunayTriangulator> gpu_;
};
#endif // USE_BGFX

}

#endif // TRIANGULATION_BACKEND_H