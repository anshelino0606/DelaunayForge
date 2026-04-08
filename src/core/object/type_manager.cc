#include "type_manager.h"
#include "object.h"
#include "type_info.h"
#include "log_categories.h"
#include <cassert>

namespace fem {

void TypeManager::cleanup() {
    for (const TypeInfo* type_info : s_type_infos_)
        type_info->cleanup();
}

const TypeInfo* TypeManager::get_type_info(const char* typeName) {
    auto it = s_index_by_type_name_.find(typeName);
    if (it == s_index_by_type_name_.end())
        return nullptr;
    return s_type_infos_.at(it->second);
}

Object* TypeManager::create_object(const ObjectTypeInfo* type_info) {
    assert(type_info);
    return type_info->get_allocator_handler()();
}

Object* TypeManager::create_object_by_name(const char* type_name) {
    const TypeInfo* type_info = get_type_info(type_name);
    assert(type_info);

    if (type_info->type() != ReflectedObjectType::OBJECT) {
        LOGT_ERROR(LogReflection, "TypeManager::create_object_by_name(): Can't be used if type is not derived from Object!");
        return nullptr;
    }

    const ObjectTypeInfo* obj_info = static_cast<const ObjectTypeInfo*>(type_info);
    return obj_info->get_allocator_handler()();
}

void TypeManager::destroy_object(Object* object) {
    assert(object);
    const ObjectTypeInfo* type_info = object->get_type_info();
    type_info->get_destructor_handler()(object);
}

void TypeManager::register_type(const TypeInfo* type_info) {
    assert(type_info);

    auto it = s_index_by_type_name_.find(type_info->get_name_c_str());
    if (it != s_index_by_type_name_.end())
        return;

    s_type_infos_.push_back(type_info);
    s_index_by_type_name_[type_info->get_name_c_str()] = (uint32_t)s_type_infos_.size() - 1;
}

}