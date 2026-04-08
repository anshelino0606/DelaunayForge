#ifndef FEM_ENTITY_H
#define FEM_ENTITY_H

#include "core/object/object.h"
#include "core/object/property.h"

namespace fem {

class World;
class Component;

class Entity : public Object
{
    FEM_DECLARE_OBJECT(Entity);
    FEM_DECLARE_PROPERTY_REGISTER(Entity);

public:
    Entity();
    ~Entity();

    virtual void init() { }

    void set_name(const std::string& name) { name_ = name; }
    const std::string& get_name() const { return name_;}

    virtual void on_world_set(World* world);

    Entity* create_child();
    Entity* create_child(const ObjectTypeInfo* type_info);

    template<typename T>
    T* create_child() {
        return static_cast<T*>(create_child(T::get_static_type_info()));
    }

    const std::vector<Entity*>& get_children() const { return children_; }

    virtual Component* create_component(const ObjectTypeInfo* type_info);
    
    template<typename T>
    T* create_component() {
        return static_cast<T*>(create_component(T::get_static_type_info()));
    }

    virtual void remove_component(Component* component);

    const std::vector<Component*>& get_components() const { return components_; }
    Component* get_component(const ObjectTypeInfo* typeInfo) const;

    template<typename T>
    T* get_component() const {
        return static_cast<T*>(get_component(T::get_static_type_info()));
    }

    void set_root(Entity* entity) { root_entity_ = entity; }
    Entity* get_root() const { return root_entity_; }

    glm::vec3 get_position() const { return position_; }
    glm::vec3 get_rotation() const { return rotation_; }
    glm::vec3 get_scale() const { return scale_; }

    void update_world_transform();

    glm::vec3 get_world_position() const;
    glm::mat4x4 get_local_transform() const;
    const glm::mat4x4& get_world_transform() const { return world_transform_; }
    const glm::mat4x4& get_prev_world_transform() const { return prev_world_transform_; }

    void translate(const glm::vec3& delta_position);
    void set_position(const glm::vec3& position) { position_ = position; }
    void set_scale(const glm::vec3& scale) { scale_ = scale; }
    void set_rotation(const glm::vec3& new_rotation);
    // TODO: Think about rotation API

    virtual void serialize() override;
    virtual void deserialize(Archive& archive) override;

protected:
    std::string name_ = "undefined";

    std::vector<Component*> components_;
    std::vector<Entity*> children_;

    World* world_ = nullptr;
    Entity* root_entity_ = nullptr;
    
    glm::mat4x4 world_transform_;
    glm::mat4x4 prev_world_transform_;

    glm::vec3 position_;
    glm::vec3 rotation_;
    glm::vec3 scale_;
};

}

#endif // FEM_ENTITY_H