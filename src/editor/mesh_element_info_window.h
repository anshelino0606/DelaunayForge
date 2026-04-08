#ifndef MESH_ELEMENT_INFO_WINDOW
#define MESH_ELEMENT_INFO_WINDOW

#include <glm/glm.hpp>
#include <imgui/imgui.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "tools/canvas_inspector.h"
#include "math/fem/fem_mesh.h"
#include "math/fem/fem_problem.h"

namespace fem {

struct DelaunayTriangulationResult;
class PlanarMeshComponent;
class PDEComponent;

class MeshElementInfoWindow final {
public:
    struct DrawInfo {
        const PlanarMeshComponent* mesh = nullptr;
        const PDEComponent*        pde  = nullptr;
        CanvasInspector::Selection sel;
    };

    void draw(const DrawInfo& info);

    bool visible = true;

private:
    void draw_vertex_(const DelaunayTriangulationResult& R, int vid);
    void draw_edge_(const DelaunayTriangulationResult& R, int eid);
    void draw_triangle_(const DelaunayTriangulationResult& R,
                                            int tid,
                                            const FEMProblem& prob);

    void ensure_cache_(const PlanarMeshComponent& mesh);
    void rebuild_bc_maps_();

    static std::uint64_t edge_key_(int a, int b) noexcept;

private:
    const PlanarMeshComponent* cached_mesh_ = nullptr;
    std::size_t cached_point_count_ = 0;
    std::size_t cached_tri_count_   = 0;
    std::vector<double> nodal_mass_;

    FEMMesh cached_fem_;
    std::unordered_map<std::uint64_t, FEMMesh::EdgeBC> edge_bc_;

    std::vector<std::uint8_t> is_dir_;
    std::vector<double>       dir_val_;
};

} // namespace fem

#endif
