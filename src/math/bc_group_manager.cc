#include "bc_group_manager.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "geom/delaunay/delaunay_types.h"
#include "log_categories.h"
#include <algorithm>
#include <unordered_map>
#include <queue>

namespace fem {


FEM_DEFINE_OBJECT(BoundaryConditionGroup, Object);

FEM_BEGIN_PROPERTY_REGISTER(BoundaryConditionGroup)
{
    FEM_REGISTER_PROPERTY(BoundaryConditionGroup, name_);
    FEM_REGISTER_PROPERTY(BoundaryConditionGroup, edge_ids_, NoUI());
    FEM_REGISTER_PROPERTY(BoundaryConditionGroup, bc_template_);
    FEM_REGISTER_PROPERTY(BoundaryConditionGroup, is_selected_, NoUI());
    FEM_REGISTER_PROPERTY(BoundaryConditionGroup, is_visible_);
}
FEM_END_PROPERTY_REGISTER(BoundaryConditionGroup);

void BoundaryConditionGroup::add_edge_id(int edge_id) {
    if (std::find(edge_ids_.begin(), edge_ids_.end(), edge_id) == edge_ids_.end()) {
        edge_ids_.push_back(edge_id);
    }
}

void BoundaryConditionGroup::add_edge_ids(const std::vector<int>& edge_ids) {
    for (int eid : edge_ids) {
        add_edge_id(eid);
    }
}

void BoundaryConditionGroup::remove_edge_id(int edge_id) {
    auto it = std::find(edge_ids_.begin(), edge_ids_.end(), edge_id);
    if (it != edge_ids_.end()) {
        edge_ids_.erase(it);
    }
}

void BoundaryConditionGroup::apply_to_mesh(PlanarMeshComponent* mesh) {
    if (!mesh || !bc_template_) return;

    BoundaryCondition* bc = nullptr;
    
    for (BoundaryCondition* existing : mesh->boundary_conditions()) {
        if (existing && existing->edge_ids() == edge_ids_) {
            bc = existing;
            break;
        }
    }

    if (!bc) {
        return;
    }

    bc->set_type(bc_template_->type());
    bc->set_value(bc_template_->value());
    bc->set_robin_alpha(bc_template_->robin_alpha());
    bc->set_robin_beta(bc_template_->robin_beta());
}

FEM_DEFINE_OBJECT(BCGroupManager, Object);

FEM_BEGIN_PROPERTY_REGISTER(BCGroupManager)
{
    FEM_REGISTER_PROPERTY(BCGroupManager, groups_);
}
FEM_END_PROPERTY_REGISTER(BCGroupManager);

BoundaryConditionGroup* BCGroupManager::create_group(const std::string& name) {
    BoundaryConditionGroup* group = create_subobject<BoundaryConditionGroup>();
    
    std::string group_name = name;
    if (group_name.empty()) {
        group_name = "Group " + std::to_string(groups_.size() + 1);
    }
    group->set_name(group_name);
    
    groups_.push_back(group);
    return group;
}

void BCGroupManager::remove_group(BoundaryConditionGroup* group) {
    auto it = std::find(groups_.begin(), groups_.end(), group);
    if (it != groups_.end()) {
        destroy_object(*it);
        groups_.erase(it);
    }
}

BoundaryConditionGroup* BCGroupManager::find_group_by_name(const std::string& name) {
    for (BoundaryConditionGroup* group : groups_) {
        if (group && group->name() == name) {
            return group;
        }
    }
    return nullptr;
}

void BCGroupManager::select_group(BoundaryConditionGroup* group, bool exclusive) {
    if (!group) return;
    
    if (exclusive) {
        deselect_all_groups();
    }
    group->set_selected(true);
}

void BCGroupManager::deselect_group(BoundaryConditionGroup* group) {
    if (group) {
        group->set_selected(false);
    }
}

void BCGroupManager::toggle_group_selection(BoundaryConditionGroup* group) {
    if (group) {
        group->set_selected(!group->is_selected());
    }
}

void BCGroupManager::select_all_groups() {
    for (BoundaryConditionGroup* group : groups_) {
        if (group) {
            group->set_selected(true);
        }
    }
}

void BCGroupManager::deselect_all_groups() {
    for (BoundaryConditionGroup* group : groups_) {
        if (group) {
            group->set_selected(false);
        }
    }
}

std::vector<BoundaryConditionGroup*> BCGroupManager::get_selected_groups() const {
    std::vector<BoundaryConditionGroup*> selected;
    for (BoundaryConditionGroup* group : groups_) {
        if (group && group->is_selected()) {
            selected.push_back(group);
        }
    }
    return selected;
}

BoundaryConditionGroup* BCGroupManager::create_group_from_selected_edges(
    const std::vector<int>& edge_ids,
    const std::string& name
) {
    if (edge_ids.empty()) {
        return nullptr;
    }

    BoundaryConditionGroup* group = create_group(name);
    group->add_edge_ids(edge_ids);
    
    // Template creation requires proper object hierarchy
    // Leave template null for now
    
    return group;
}

std::vector<BCGroupManager::BoundaryLoop> BCGroupManager::detect_all_boundary_loops(
    const DelaunayTriangulationResult& triangulation
) {
    // Build adjacency graph of boundary edges
    std::unordered_map<int, std::vector<std::pair<int, int>>> vertex_to_edges;
    
    for (size_t i = 0; i < triangulation.edges.size(); ++i) {
        const EdgeInfo& edge = triangulation.edges[i];
        if (!edge.on_boundary) continue;
        
        vertex_to_edges[edge.a].emplace_back(edge.b, (int)i);
        vertex_to_edges[edge.b].emplace_back(edge.a, (int)i);
    }

    std::vector<BoundaryLoop> loops;
    std::unordered_set<int> visited_edges;

    // Find all loops
    for (const auto& [start_vertex, neighbors] : vertex_to_edges) {
        if (neighbors.empty()) continue;

        // Try to trace a loop from this vertex
        int current = start_vertex;
        int prev = -1;
        BoundaryLoop loop;
        std::unordered_set<int> loop_visited;

        bool found_loop = false;
        while (true) {
            const auto& edges = vertex_to_edges[current];
            
            // Find an unvisited edge
            bool found_next = false;
            for (const auto& [next_vertex, edge_id] : edges) {
                // Skip if we came from this vertex
                if (next_vertex == prev) continue;
                
                // Skip if this edge was already used in a loop
                if (visited_edges.count(edge_id) > 0) continue;
                
                // Skip if we've already visited this edge in current loop
                if (loop_visited.count(edge_id) > 0) {
                    // We've closed a loop!
                    found_loop = true;
                    break;
                }

                loop.vertex_ids.push_back(current);
                loop.edge_ids.push_back(edge_id);
                loop_visited.insert(edge_id);
                
                prev = current;
                current = next_vertex;
                found_next = true;
                break;
            }

            if (found_loop || !found_next) {
                break;
            }

            // Check if we've returned to start
            if (current == start_vertex && loop.vertex_ids.size() > 2) {
                found_loop = true;
                break;
            }

            // Safety limit
            if (loop.vertex_ids.size() > triangulation.points.size()) {
                break;
            }
        }

        if (found_loop && loop.edge_ids.size() >= 3) {
            // Mark all edges as visited
            for (int eid : loop.edge_ids) {
                visited_edges.insert(eid);
            }
            loops.push_back(loop);
        }
    }

    // Determine which loop is outer (largest area)
    if (loops.size() > 1) {
        double max_area = -1e300;
        int outer_idx = 0;

        for (size_t i = 0; i < loops.size(); ++i) {
            double area = 0.0;
            const auto& vids = loops[i].vertex_ids;
            
            for (size_t j = 0; j < vids.size(); ++j) {
                int v0 = vids[j];
                int v1 = vids[(j + 1) % vids.size()];
                
                if (v0 < 0 || v0 >= (int)triangulation.points.size()) continue;
                if (v1 < 0 || v1 >= (int)triangulation.points.size()) continue;
                
                const Point2D& p0 = triangulation.points[v0];
                const Point2D& p1 = triangulation.points[v1];
                area += p0.x() * p1.y() - p1.x() * p0.y();
            }
            
            area = std::abs(area * 0.5);
            if (area > max_area) {
                max_area = area;
                outer_idx = (int)i;
            }
        }

        loops[outer_idx].is_outer = true;
    } else if (loops.size() == 1) {
        loops[0].is_outer = true;
    }

    return loops;
}

void BCGroupManager::auto_detect_inner_boundaries(
    const DelaunayTriangulationResult& triangulation,
    const std::string& group_name
) {
    std::vector<BoundaryLoop> loops = detect_all_boundary_loops(triangulation);

    int inner_count = 0;
    for (const BoundaryLoop& loop : loops) {
        if (loop.is_outer) continue;

        std::string name = group_name;
        if (inner_count > 0) {
            name += " " + std::to_string(inner_count + 1);
        }

        BoundaryConditionGroup* group = create_group_from_selected_edges(loop.edge_ids, name);
        inner_count++;

        LOGT_INFO(LogMath, "Auto-detected inner boundary group '%s' with %zu edges", 
                  name.c_str(), loop.edge_ids.size());
    }

    if (inner_count > 0) {
        LOGT_INFO(LogMath, "Created %d inner boundary groups", inner_count);
    }
}

void BCGroupManager::auto_detect_outer_boundary(
    const DelaunayTriangulationResult& triangulation,
    const std::string& group_name
) {
    std::vector<BoundaryLoop> loops = detect_all_boundary_loops(triangulation);

    for (const BoundaryLoop& loop : loops) {
        if (!loop.is_outer) continue;

        BoundaryConditionGroup* group = create_group_from_selected_edges(loop.edge_ids, group_name);
        
        LOGT_INFO(LogMath, "Auto-detected outer boundary group '%s' with %zu edges",
                  group_name.c_str(), loop.edge_ids.size());
        break;
    }
}

void BCGroupManager::apply_bc_to_selected_groups(BoundaryConditionType type, double value, double beta) {
    for (BoundaryConditionGroup* group : groups_) {
        if (!group || !group->is_selected()) continue;

        if (!group->boundary_condition_template()) {
            // TODO: BC creation requires public API from mesh
            continue;
        }

        BoundaryCondition* bc = group->boundary_condition_template();
        bc->set_type(type);
        bc->set_value(value);
        if (type == BoundaryConditionType::Robin) {
            bc->set_robin_beta(beta);
        }
    }
}

void BCGroupManager::apply_bc_to_all_groups(BoundaryConditionType type, double value, double beta) {
    for (BoundaryConditionGroup* group : groups_) {
        if (!group) continue;

        if (!group->boundary_condition_template()) {
            // TODO: BC creation requires public API from mesh
            continue;
        }

        BoundaryCondition* bc = group->boundary_condition_template();
        bc->set_type(type);
        bc->set_value(value);
        if (type == BoundaryConditionType::Robin) {
            bc->set_robin_beta(beta);
        }
    }
}

void BCGroupManager::apply_all_to_mesh(PlanarMeshComponent* mesh) {
    if (!mesh) return;

    for (BoundaryConditionGroup* group : groups_) {
        if (group && group->is_visible()) {
            group->apply_to_mesh(mesh);
        }
    }
}

int BCGroupManager::get_total_edge_count() const {
    int count = 0;
    for (const BoundaryConditionGroup* group : groups_) {
        if (group) {
            count += (int)group->edge_ids().size();
        }
    }
    return count;
}

std::unordered_set<int> BCGroupManager::get_all_grouped_edge_ids() const {
    std::unordered_set<int> all_edges;
    for (const BoundaryConditionGroup* group : groups_) {
        if (group) {
            for (int eid : group->edge_ids()) {
                all_edges.insert(eid);
            }
        }
    }
    return all_edges;
}

} // namespace fem
