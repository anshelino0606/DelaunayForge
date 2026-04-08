#ifndef FEM_COMPONENT_H
#define FEM_COMPONENT_H

#include "core/object/object.h"
#include "core/object/property.h"

namespace fem {

class World;
class Entity;

class Component : public Object
{
    FEM_DECLARE_OBJECT(Component);
    FEM_DECLARE_PROPERTY_REGISTER(Component);

public:
    virtual void update(float deltaTime) { }

    void set_active(bool active) { is_active_ = active; }
    bool is_active() const { return is_active_; }

    virtual void on_init() { }
    virtual void on_entity_set(Entity* entity) { entity_ = entity; }
    virtual void on_world_set(World* world) { world_ = world; }
    
    virtual void on_cleanup() { }
    virtual void on_entity_remove() { entity_ = nullptr; }
    virtual void on_world_remove() { world_ = nullptr; }

    Entity* get_entity() const { return entity_; }
    World* get_world() const { return world_; }

protected:
    Entity* entity_ = nullptr;
    World* world_ = nullptr;

    bool is_active_ = false;
};

}

#endif // FEM_COMPONENT_H