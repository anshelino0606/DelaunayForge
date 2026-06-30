#include "outliner_window.h"
#include "events.h"
#include "core/entity/entity.h"
#include "core/entity/events.h"
#include "math/entities/planar_math_entity.h"
#include "geom/mesh/mesh_component.h"
#include <imgui/imgui.h>

namespace fem {

void OutlinerWindow::draw(const OutlinerWindowDrawInfo& draw_info)
{
    assert(draw_info.entities);

    const std::vector<Entity*>& entities = *draw_info.entities;

    if (!entities.empty() && !last_selected_entity_) {
        select_entity(entities[0]);
    }

    all_drawn_entities_.clear();
    any_item_hovered_ = false;

    ImGui::Begin("Outliner");

    for (Entity* entity : entities)
    {
        if (!entity->get_root())
            draw_node(entity);
    }

    if (ImGui::BeginPopupContextWindow(
        "OutlinerContextMenu",
        ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight
    ))
    {
        if (ImGui::MenuItem("Create Planar Math Entity")) {
            EntityCreationRequest request(PlanarMathEntity::get_static_type_info());
            request.enqueue();
        }

        ImGui::EndPopup();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
        remove_selected_entities();

    handle_shift_selection();

    ImGui::End();
}

void OutlinerWindow::draw_node(Entity* entity) {
    MathEntity* math_entity = nullptr;

    if (entity->is_a<MathEntity>()) {
        math_entity = static_cast<MathEntity*>(entity);
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    all_drawn_entities_.push_back(entity);

    // if (entity->get_children().empty())
    //     flags |= ImGuiTreeNodeFlags_Leaf;

    if (selected_entities_.contains(entity)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool is_node_opened = false;

    ImGui::PushID(entity);

    if (renamed_entity_ == entity) {
        char buffer[128];
        strncpy(buffer, entity->get_name().c_str(), sizeof(buffer));
        ImGui::SetKeyboardFocusHere();

        if (ImGui::InputText("##rename", buffer, IM_ARRAYSIZE(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            entity->set_name(buffer);
            renamed_entity_ = nullptr;
        }

        is_node_opened = ImGui::TreeNodeEx("##hidden", flags, "");
    }
    else
    {
        is_node_opened = ImGui::TreeNodeEx(entity->get_name().c_str(), flags);

        if (ImGui::IsItemClicked()) {
            handle_click(entity);
            renamed_entity_ = nullptr;
        }
        
        if (ImGui::BeginPopupContextItem()) {
            if (math_entity) {
                if (ImGui::MenuItem("Add Mesh")) {
                    math_entity->add_mesh();
                }
                ImGui::Separator();
            }

            if (!selected_entities_.contains(entity))
                selected_entities_.clear();

            if (ImGui::MenuItem("Rename"))
                renamed_entity_ = entity;

            ImGui::Separator();

            if (ImGui::MenuItem("Remove")) {
                EntityRemovalRequest event(entity);
                event.enqueue();
                remove_selected_entities();
            }

            ImGui::EndPopup();
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F2) && entity == last_selected_entity_) {
        renamed_entity_ = entity;
    }

    if (ImGui::IsItemHovered())
        any_item_hovered_ = true;

    if (is_node_opened) {
        if (entity->is_a<MathEntity>()) {
            MathEntity* math_entity = static_cast<MathEntity*>(entity);

            for (MeshComponent* mesh_component : math_entity->mesh_components()) {
                draw_node(mesh_component);
            }

            if (mesh_component_to_remove_) {
                bool update_selected_mesh = false;

                if (selected_mesh_component_ == mesh_component_to_remove_) {
                    update_selected_mesh = true;
                }

                math_entity->remove_component(mesh_component_to_remove_);
                mesh_component_to_remove_ = nullptr;

                if (update_selected_mesh && !math_entity->mesh_components().empty()) {
                    selected_mesh_component_ = math_entity->mesh_components()[0];
                } else {
                    selected_mesh_component_ = nullptr;
                }
            }
        }

        for (Entity* child : entity->get_children())
            draw_node(child);
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void OutlinerWindow::draw_node(MeshComponent* mesh) {
    ImGui::PushID(mesh);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;

    if (mesh == selected_mesh_component_) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::TreeNodeEx("Mesh", flags);

    if (ImGui::IsItemClicked()) {
        if (selected_mesh_component_) {
            if (selected_mesh_component_->get_entity() != mesh->get_entity()) {
                deselect_entity(selected_mesh_component_->get_entity());
                select_entity(mesh->get_entity());
            }
        }

        selected_mesh_component_ = mesh;
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Remove")) {
            mesh_component_to_remove_ = mesh;
        }

        ImGui::EndPopup();
    }

    ImGui::TreePop();

    ImGui::PopID();
}

void OutlinerWindow::handle_click(Entity* entity) {
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && last_selected_entity_)
    {   
        selected_by_shift_entity_ = entity;
    }
    else if (ImGui::IsKeyDown(ImGuiKey::ImGuiMod_Ctrl))
    {
        if (selected_entities_.contains(entity))
        {
            deselect_entity(entity);
        }
        else
        {
            select_entity(entity);
        }
    }
    else
    {
        selected_entities_.clear();
        select_entity(entity);
    }
}

void OutlinerWindow::handle_shift_selection() {
    if (!selected_by_shift_entity_ || !last_selected_entity_)
        return;

    auto it1 = std::find(all_drawn_entities_.begin(), all_drawn_entities_.end(), last_selected_entity_);
    auto it2 = std::find(all_drawn_entities_.begin(), all_drawn_entities_.end(), selected_by_shift_entity_);
    if (it1 != all_drawn_entities_.end() && it2 != all_drawn_entities_.end()) {
        if (it1 > it2) std::swap(it1, it2);
        for (auto it = it1; it <= it2; ++it) {
            select_entity((*it));
        }

        last_selected_entity_ = selected_by_shift_entity_;
        selected_by_shift_entity_ = nullptr;
    }
}

void OutlinerWindow::select_entity(Entity* entity) {
    if (selected_entities_.contains(entity))
        return;

    selected_entities_.insert(entity);
    last_selected_entity_ = entity;
    selected_mesh_component_ = last_selected_entity_->get_component<MeshComponent>();
}

void OutlinerWindow::deselect_entity(Entity* entity) {
    if (!selected_entities_.contains(entity))
        return;

    selected_entities_.erase(entity);
    last_selected_entity_ = nullptr;
}

void OutlinerWindow::remove_selected_entities() {
    for (Entity* entity : selected_entities_) {
        EntityRemovalRequest event(entity);
        event.enqueue();
    }

    selected_entities_.clear();

    last_selected_entity_ = nullptr;
}

}