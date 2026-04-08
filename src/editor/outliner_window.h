#ifndef FEM_OUTLINER_H
#define FEM_OUTLINER_H

#include <vector>
#include <unordered_set>

namespace fem {

class Entity;
class MeshComponent;

struct OutlinerWindowDrawInfo {
    const std::vector<Entity*>* entities;
};

class OutlinerWindow {
public:
    void draw(const OutlinerWindowDrawInfo& draw_info);

    Entity* last_selected_entity() const { return last_selected_entity_; }
    MeshComponent* selected_mesh_component() const { return selected_mesh_component_; }

private:
    Entity* renamed_entity_ = nullptr;
    Entity* last_selected_entity_ = nullptr;
    Entity* selected_by_shift_entity_ = nullptr;
    bool any_item_hovered_ = false;

    MeshComponent* selected_mesh_component_ = nullptr;
    MeshComponent* mesh_component_to_remove_ = nullptr;
    
    std::vector<Entity*> all_drawn_entities_;
    std::unordered_set<Entity*> selected_entities_;

    void draw_node(Entity* entity);
    void draw_node(MeshComponent* mesh);
    void handle_click(Entity* entity);
    void handle_shift_selection();
    void select_entity(Entity* entity);
    void deselect_entity(Entity* entity);
    void remove_selected_entities();

};

}

#endif // FEM_OUTLINER_H