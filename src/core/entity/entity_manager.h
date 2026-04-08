#ifndef FEM_ENTITY_MANAGER_H
#define FEM_ENTITY_MANAGER_H

#include "entity.h"

namespace fem {

class EntityManager {
public:
    void reset();

    void update();

    Entity* create_entity();
    Entity* create_entity(const ObjectTypeInfo* type_info);
    Entity* create_entity(Archive& archive);

    template<typename T>
    T* create_entity()
    {
        static_assert(std::is_base_of_v<Entity, T>);
        return static_cast<T*>(create_entity(T::get_static_type_info()));
    }

    void remove_entity(Entity* entity);
    
    const std::vector<Entity*>& get_entities() const { return entities_; }

private:
    std::vector<Entity*> entities_;
    std::vector<Entity*> entities_to_create_;
    std::vector<Entity*> entities_to_remove_;
};

}

#endif // FEM_ENTITY_MANAGER_H