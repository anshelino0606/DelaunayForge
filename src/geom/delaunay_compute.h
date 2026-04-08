#pragma once
#ifdef USE_BGFX

#include <bgfx/bgfx.h>
#include <vector>
#include <glm/glm.hpp>
#include "delaunay_types.h"
#include "rhi/compute_program.h"

namespace fem {

class DelaunayComputeManager {
public:
    struct ComputeResult {
        std::vector<float> min_angles, avg_angles, qualities, areas;
        std::vector<bool>  flip_decisions;
    };

    DelaunayComputeManager();
    ~DelaunayComputeManager();

    bool init();
    void shutdown();

    ComputeResult compute_triangle_metrics(
        const std::vector<Point2D>& points,
        const std::vector<Tri>& triangles);

    std::vector<bool> compute_edge_flips(
        const std::vector<Point2D>& points,
        const std::vector<Tri>& triangles);

private:
    fem::ComputeProgram triangle_quality_program_;
    fem::ComputeProgram edge_flip_program_;
    bgfx::UniformHandle u_params_                = BGFX_INVALID_HANDLE;

    bool initialized_ = false;
};

}

#endif // USE_BGFX