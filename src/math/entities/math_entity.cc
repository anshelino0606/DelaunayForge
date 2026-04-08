#include "math_entity.h"
#include "math/pde/pde_component.h"
#include "geom/planar_mesh/planar_mesh_component.h"

namespace fem {

FEM_DEFINE_OBJECT(MathEntity, Entity);
FEM_BEGIN_PROPERTY_REGISTER(MathEntity)
{

}
FEM_END_PROPERTY_REGISTER(MathEntity)

Component* MathEntity::create_component(const ObjectTypeInfo* type_info) {
    Component* component = Entity::create_component(type_info);
    
    if (component->is_a<MeshComponent>()) {
        mesh_components_.push_back(static_cast<MeshComponent*>(component));
    }

    if (component->is_a<PDEComponent>()) {
        pde_component_ = static_cast<PDEComponent*>(component);
    }

    return component;
}

void MathEntity::remove_component(Component* component) {
    if (component->is_a<MeshComponent>()) {
        auto it = std::find(mesh_components_.begin(), mesh_components_.end(), component);

        if (it != mesh_components_.end()) {
            mesh_components_.erase(it);
        }
    }

    Entity::remove_component(component);
}

void MathEntity::deserialize(Archive& archive) {
    Entity::deserialize(archive);

    for (Component* component : components_) {
        if (component->is_a<PDEComponent>()) {
            pde_component_ = static_cast<PDEComponent*>(component);
        } else if (component->is_a<MeshComponent>()) {
            mesh_components_.push_back(static_cast<MeshComponent*>(component));
            mesh_components_.back()->update_buffers();
        }
    }
}

}