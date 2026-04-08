#ifndef FEM_BC_GROUP_MANAGER_H
#define FEM_BC_GROUP_MANAGER_H

#include "core/object/object.h"
#include "boundary_condition.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <memory>

namespace fem {

class PlanarMeshComponent;
struct DelaunayTriangulationResult;

class BoundaryConditionGroup : public Object {
public:
    FEM_DECLARE_OBJECT(BoundaryConditionGroup);
    FEM_DECLARE_PROPERTY_REGISTER(BoundaryConditionGroup);

    BoundaryConditionGroup() = default;
    virtual ~BoundaryConditionGroup() override = default;

    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    const std::vector<int>& edge_ids() const { return edge_ids_; }
    void add_edge_id(int edge_id);
    void add_edge_ids(const std::vector<int>& edge_ids);
    void remove_edge_id(int edge_id);
    void clear_edge_ids() { edge_ids_.clear(); }

    BoundaryCondition* boundary_condition_template() const { return bc_template_; }
    void set_boundary_condition_template(BoundaryCondition* bc) { bc_template_ = bc; }

    bool is_selected() const { return is_selected_; }
    void set_selected(bool selected) { is_selected_ = selected; }

    bool is_visible() const { return is_visible_; }
    void set_visible(bool visible) { is_visible_ = visible; }

    void apply_to_mesh(PlanarMeshComponent* mesh);

protected:
    std::string name_ = "Boundary Group";
    std::vector<int> edge_ids_;
    BoundaryCondition* bc_template_ = nullptr;
    bool is_selected_ = false;
    bool is_visible_ = true;
};


class BCGroupManager : public Object {
public:
    FEM_DECLARE_OBJECT(BCGroupManager);
    FEM_DECLARE_PROPERTY_REGISTER(BCGroupManager);

    BCGroupManager() = default;
    virtual ~BCGroupManager() override = default;

    // Group management
    BoundaryConditionGroup* create_group(const std::string& name = "");
    void remove_group(BoundaryConditionGroup* group);
    void clear_groups() { groups_.clear(); }
    
    const std::vector<BoundaryConditionGroup*>& groups() const { return groups_; }
    BoundaryConditionGroup* find_group_by_name(const std::string& name);

    void select_group(BoundaryConditionGroup* group, bool exclusive = true);
    void deselect_group(BoundaryConditionGroup* group);
    void toggle_group_selection(BoundaryConditionGroup* group);
    void select_all_groups();
    void deselect_all_groups();
    std::vector<BoundaryConditionGroup*> get_selected_groups() const;

    BoundaryConditionGroup* create_group_from_selected_edges(
        const std::vector<int>& edge_ids, 
        const std::string& name = ""
    );

    void auto_detect_inner_boundaries(
        const DelaunayTriangulationResult& triangulation,
        const std::string& group_name = "Inner Boundaries"
    );

    void auto_detect_outer_boundary(
        const DelaunayTriangulationResult& triangulation,
        const std::string& group_name = "Outer Boundary"
    );

    void apply_bc_to_selected_groups(BoundaryConditionType type, double value, double beta = 0.0);
    void apply_bc_to_all_groups(BoundaryConditionType type, double value, double beta = 0.0);

    void apply_all_to_mesh(PlanarMeshComponent* mesh);

    int get_total_edge_count() const;
    std::unordered_set<int> get_all_grouped_edge_ids() const;

protected:
    std::vector<BoundaryConditionGroup*> groups_;

private:

    struct BoundaryLoop {
        std::vector<int> vertex_ids;
        std::vector<int> edge_ids;
        bool is_outer = false;
    };

    static std::vector<BoundaryLoop> detect_all_boundary_loops(
        const DelaunayTriangulationResult& triangulation
    );
};

} // namespace fem

#endif // FEM_BC_GROUP_MANAGER_H
