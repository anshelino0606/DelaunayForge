#ifndef FEM_MATH_ENTITY_H
#define FEM_MATH_ENTITY_H

#include "core/entity/entity.h"

namespace fem {

class MeshComponent;
class PDEComponent;

class MathEntity : public Entity {
public:
    FEM_DECLARE_OBJECT(MathEntity);
    FEM_DECLARE_PROPERTY_REGISTER(MathEntity);

    using Entity::create_component;

    PDEComponent* pde_component() const { return pde_component_; }
    const std::vector<MeshComponent*>& mesh_components() const { return mesh_components_; }

    virtual MeshComponent* add_mesh() { return nullptr; }

    virtual Component* create_component(const ObjectTypeInfo* type_info) override;
    virtual void remove_component(Component* component) override;
    virtual void deserialize(Archive& archive) override;

protected:
    PDEComponent* pde_component_ = nullptr;
    std::vector<MeshComponent*> mesh_components_;
};

}

#endif // FEM_MATH_ENTITY_H