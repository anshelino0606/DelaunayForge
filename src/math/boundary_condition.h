#ifndef FEM_BOUNDARY_CONDITION_H
#define FEM_BOUNDARY_CONDITION_H

#include "core/object/object.h"
#include "core/object/property.h"

#include <vector>
#include <unordered_map>
#include <utility>

namespace fem {

struct DelaunayTriangulationResult;
class PlanarMeshComponent;

FEM_DECLARE_ENUM(BoundaryConditionType, None, Dirichlet, Neumann, Robin);
FEM_DECLARE_ENUM(BoundaryConditionPathMode, Shorter, Longer, CW, CCW);

class BoundaryCondition : public Object {
public:
    FEM_DECLARE_OBJECT(BoundaryCondition);
    FEM_DECLARE_PROPERTY_REGISTER(BoundaryCondition);

    void apply(DelaunayTriangulationResult& triangulation_result) const;
    void reset();

    void set_edge_ids(const std::vector<int>& edge_ids);
    void set_start_point(int start_point);
    void set_end_point(int end_point);
    void switch_path_mode_to_alternative();

    void rebuild();

    double value() const { return value_; }
    double robin_alpha() const { return value_; }
    double robin_beta()  const { return value_beta_; }

    void set_value(double value) { value_ = value; }
    void set_robin_alpha(double alpha) { value_ = alpha; }
    void set_robin_beta(double beta) { value_beta_ = beta; }
    void set_type(BoundaryConditionType type) { type_ = type; }

    int start_point() const { return start_point_; }
    int end_point() const { return end_point_; }

    BoundaryConditionType type() const { return type_; }
    BoundaryConditionPathMode path_mode() const { return path_mode_; }
    const std::vector<int>& edge_ids() const { return edge_ids_; }
    
    void remap_after_retriangulation();
    void capture_geometry_from_edges();
    void capture_geometry_from_edges(const DelaunayTriangulationResult& R);

    bool is_selected_ = false;

    static std::vector<int> compute_boundary_arc_edges(
        const DelaunayTriangulationResult& R,
        int v0,
        int v1,
        BoundaryConditionPathMode mode
    );

    static bool extract_boundary_loop(
        const DelaunayTriangulationResult& R,
        int v_start,
        std::vector<int>& loop_vs,
        std::vector<int>& loop_eids
    );

    static std::vector<int> arc_between_on_loop(
        const std::vector<int>& loop_vs,
        const std::vector<int>& loop_eids,
        int v0, 
        int v1,
        bool forward
    );

    static double polygon_area_sign(
        const DelaunayTriangulationResult& R,
        const std::vector<int>& loop_vs
    );

    static std::unordered_map<int, std::vector<std::pair<int,int>>>
        build_boundary_graph(const DelaunayTriangulationResult& R);

    static std::vector<int>
        find_boundary_path_edge_ids(const DelaunayTriangulationResult& R, int v0, int v1);


protected:
    double value_ = 0.0;
    double value_beta_ = 0.0;
    std::vector<int> edge_ids_;
    BoundaryConditionType type_ = BoundaryConditionType::None;
    BoundaryConditionPathMode path_mode_ = BoundaryConditionPathMode::Shorter;

    static constexpr int s_invalid = -1;

    int start_point_ = s_invalid;
    int end_point_ = s_invalid;

    int loop_index_ = s_invalid;
    double start_s_ = 0.0;
    double end_s_ = 0.0;
    bool has_param_ = false;
    
    std::vector<glm::dvec2> arc_positions_;
    bool has_geometry_ = false;

    void begin_selection();
    void apply_selection();
    void clear_selection();
    void cancel_selection();

    PlanarMeshComponent* mesh_component() const;
    const DelaunayTriangulationResult& triangulation_result() const;
};

}

#endif // FEM_BOUNDARY_CONDITION_H