#include "world.h"
#include "events.h"

namespace fem {

FEM_DEFINE_OBJECT(World, Object);

World::World() {
    EntityCreationRequest::subscribe([this](const EntityCreationRequest& request){
        create_entity(request.entity_type_info());
    });
}

void World::reset() {
    entity_manager_.reset();
}

Entity* World::create_entity() {
    Entity* entity = entity_manager_.create_entity();
    entity->on_world_set(this);
    entity->init();
    return entity;
}

Entity* World::create_entity(const ObjectTypeInfo* typeInfo) {
    Entity* entity = entity_manager_.create_entity(typeInfo);
    entity->on_world_set(this);
    entity->init();
    return entity;
}

Entity* World::create_entity(Archive& archive) {
    Entity* entity = entity_manager_.create_entity(archive);
    entity->on_world_set(this);
    entity->init();

    return entity;
}

void World::remove_entity(Entity* entity) {
    entity_manager_.remove_entity(entity);
}

void World::save_entities() {
    for (Entity* entity : get_entities()) {
        entity->serialize();
    }
}

void World::update_pre_entities_update() {
    entity_manager_.update();

    const std::vector<Entity*>& entities = entity_manager_.get_entities();
    for (Entity* entity : entities) {
        entity->update_world_transform();
    }
}

}