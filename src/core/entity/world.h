#ifndef FEM_WORLD_H
#define FEM_WORLD_H

#include "entity_manager.h"

namespace fem {

class World : public Object {
    FEM_DECLARE_OBJECT(World);

public:
    World();

    void reset();

    Entity* create_entity();
    Entity* create_entity(const ObjectTypeInfo* typeInfo);
    Entity* create_entity(Archive& archive);

    template<typename T>
    T* create_child() {
        static_assert(std::is_base_of_v<Entity, T>);
        return static_cast<T*>(create_entity(T::get_static_type_info()));
    }

    void remove_entity(Entity* entity);

    void save_entities();

    void update_pre_entities_update();

    EntityManager& get_entity_manager() { return entity_manager_; }
    const EntityManager& get_entity_manager() const { return entity_manager_; }

    const std::vector<Entity*>& get_entities() const { return entity_manager_.get_entities(); }

private:
    EntityManager entity_manager_;
};

}

#endif // FEM_WORLD_H