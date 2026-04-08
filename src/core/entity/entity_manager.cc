#include "entity_manager.h"

namespace fem {

void EntityManager::reset() {
    for (Entity* entity : entities_) {
        destroy_object(entity);
    }

    entities_.clear();
}

void EntityManager::update()
{
    for (Entity* entity : entities_to_remove_) {
        auto it = std::find(entities_.begin(), entities_.end(), entity);
        if (it != entities_.end()) {
            destroy_object(entity);
            entities_.erase(it);
        }
    }

    entities_to_remove_.clear();

    for (Entity* entity : entities_to_create_) {
        entities_.push_back(entity);
    }

    entities_to_create_.clear();
}

Entity* EntityManager::create_entity()
{
    entities_to_create_.push_back(create_object<Entity>());
    return entities_to_create_.back();
}

Entity* EntityManager::create_entity(const ObjectTypeInfo* typeInfo)
{
    if (typeInfo->is_exactly(Entity::get_static_type_info()))
        return create_entity();

    Entity* entity = static_cast<Entity*>(create_object(typeInfo));
    entities_to_create_.push_back(entity);

    return entity;
}

Entity* EntityManager::create_entity(Archive& archive) {
    entities_.push_back(create_object<Entity>(archive));
    return entities_.back();
}

void EntityManager::remove_entity(Entity* entity)
{
    entities_to_remove_.push_back(entity);
}

}