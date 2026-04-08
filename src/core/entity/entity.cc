#include "entity.h"
#include "world.h"
#include "component.h"
#include "core/file_system/file_system.h"
#include "core/project_file_extensions.h"
#include <glm/gtc/matrix_transform.hpp>

namespace fem {

FEM_DEFINE_OBJECT(Entity, Object)

FEM_BEGIN_PROPERTY_REGISTER(Entity)
{
    // Disable for now because no 3D view
    // FEM_REGISTER_PROPERTY(Entity, world_transform_);
    // FEM_REGISTER_PROPERTY(Entity, position_);
    // FEM_REGISTER_PROPERTY(Entity, rotation_);
    // FEM_REGISTER_PROPERTY(Entity, scale_);
}
FEM_END_PROPERTY_REGISTER(Entity)

Entity::Entity()
    : world_transform_(1.0f),
    prev_world_transform_(1.0f),
    position_(0),
    rotation_(0),
    scale_(1) {
    
}

Entity::~Entity() {
    for (Component* component : components_)
        destroy_object(component);

    for (Entity* entity : children_)
        world_->remove_entity(entity);

    components_.clear();
    children_.clear();
    world_ = nullptr;
    root_entity_ = nullptr;
}

void Entity::on_world_set(World* world) {
    assert(world);
    world_ = world;

    for (Component* component : components_) {
        component->on_world_set(world_);
    }
}

Entity* Entity::create_child() {
    assert(world_);
    Entity* child = children_.emplace_back(world_->create_entity());
    child->set_root(this);
    return child;
}

Entity* Entity::create_child(const ObjectTypeInfo* type_info) {
    assert(world_);
    assert(type_info);

    Entity* child = children_.emplace_back(world_->create_entity(type_info));
    child->set_root(this);
    return child;
}

Component* Entity::create_component(const ObjectTypeInfo* type_info) {
    assert(type_info);

    Component* component = static_cast<Component*>(create_object(type_info));
    components_.push_back(component);
    if (world_) component->on_world_set(world_);
    component->on_entity_set(this);

    return component;
}

void Entity::remove_component(Component* component) {
    auto it = std::find(components_.begin(), components_.end(), component);

    if (it != components_.end()) {
        destroy_object(component);
        components_.erase(it);
    }
}

Component* Entity::get_component(const ObjectTypeInfo* type_info) const {
    for (Component* component : components_)
        if (component->is_a(type_info))
            return component;

    return nullptr;
}

void Entity::update_world_transform() {
    prev_world_transform_ = world_transform_;

    glm::mat4 world_transform(1.0f);

    // TODO: Transform logic

    if (root_entity_)
        world_transform *= root_entity_->get_world_transform();

    world_transform_ = world_transform;
}

glm::vec3 Entity::get_world_position() const {
    if (root_entity_) {
        root_entity_->update_world_transform();
        return root_entity_->get_world_transform() * glm::vec4(position_, 1.0f);
    }

    return position_;
}

glm::mat4x4 Entity::get_local_transform() const {
    // TODO
    return glm::mat4(1.0);
}

void Entity::translate(const glm::vec3& delta_position) {
    position_.x += delta_position.x;
    position_.y += delta_position.y;
    position_.z += delta_position.z;
}

void Entity::set_rotation(const glm::vec3& euler_angles) {
    // TODO
}

void Entity::serialize() {
    if (!last_path_.empty()) {
        std::string last_saved_file_name = FileSystem::get_file_name(last_path_);
        if (last_saved_file_name != name_) {
            std::filesystem::remove(last_path_);
        }
    }

    last_path_ = std::format("{}/entities/{}.{}", FileSystem::get_project_path(), name_, g_entity_file_extension);

    Archive archive;

    archive << get_type_info()->get_name();
    archive << components_.size();

    for (Component* component : components_) {
        component->serialize(archive);
    }

    archive.save(last_path_);
}

void Entity::deserialize(Archive& archive) {
    size_t componentCount = 0;
    archive >> componentCount;

    components_.reserve(componentCount);

    for (size_t i = 0; i != componentCount; ++i) {
        Component* component = create_object<Component>(archive);
        component->on_entity_set(this);
        components_.push_back(component);
    }

    Object::deserialize(archive);
    name_ = FileSystem::get_file_name(archive.path());
}

}