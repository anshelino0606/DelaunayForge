#include "type_info.h"
#include "object.h"
#include "property.h"
#include "function.h"
#include "type_attribute.h"
#include "log_categories.h"

namespace fem {

TypeInfo::TypeInfo(
    const char* name,
    uint64_t size,
    uint64_t alignment
) {
    name_ = name;
    class_size_ = size;
    class_alignment_ = alignment;
    name_hash_ = std::hash<std::string>()(name);
}

std::vector<Property*> StructTypeInfo::get_properties() const {
    std::vector<Property*> properties;

    for_each_property([&properties](Property* property) {
        properties.push_back(property);
    });

    return properties;
}

std::vector<Function*> StructTypeInfo::get_functions() const {
    std::vector<Function*> functions;

    for_each_function([&functions](Function* function) {
        functions.push_back(function);
    });

    return functions;
}

Property* StructTypeInfo::get_property(std::string_view property_name) const {
    if (Member* member = get_member(property_name)) {
        switch (member->member_type()) {
        case MemberType::PROPERTY: 
            return static_cast<Property*>(member);
        case MemberType::FUNCTION:
            LOGT_ERROR(LogReflection, "StructTypeInfo::get_property(): In [%s] member [%s] is a function, not a property!", 
                name_.data(), property_name.data());
            return nullptr;
        }
    }

    return nullptr;
}

Function* StructTypeInfo::get_function(std::string_view function_name) const {
    if (Member* member = get_member(function_name)) {
        switch (member->member_type()) {
        case MemberType::PROPERTY: 
            LOGT_ERROR(LogReflection, "StructTypeInfo::get_function(): In [%s] member [%s] is a property, not a function!", 
                name_.data(), function_name.data());
            return nullptr;
        case MemberType::FUNCTION:
            return static_cast<Function*>(member);
        }
    }

    return nullptr;
}

Member* StructTypeInfo::get_member(std::string_view member_name) const {
    for (Member* member : members_)
        if (member_name == member->raw_name())
            return member;

    LOGT_ERROR(LogReflection, "StructTypeInfo::get_member(): Type [] does not have member [%s]!", name_.data(), member_name.data());
    return nullptr;
}

void StructTypeInfo::for_each_property(const ForEachPropertyHandler& callback) const {
    for (Member* member : members_) {
        if (member->member_type() == MemberType::PROPERTY) {
            callback(static_cast<Property*>(member));
        }
    }
}

void StructTypeInfo::for_each_function(const ForEachFunctionHandler& callback) const {
    for (Member* member : members_) {
        if (member->member_type() == MemberType::FUNCTION) {
            callback(static_cast<Function*>(member));
        }
    }
}

void StructTypeInfo::add_member(Member* member) const {
    bool is_found = false;

    for (Member* added_member : members_) {
        if (added_member->raw_name() == member->raw_name()) {
            is_found = true;
            break;
        }
    }

    if (!is_found) {
        members_.push_back(member);
    }
}

void StructTypeInfo::cleanup() const {
    for (Member* member : members_) {
        delete member;
    }

    members_.clear();
}

ObjectTypeInfo::ObjectTypeInfo(
    const char* name,
    AllocatorHandler allocator_handler,
    DestructorHandler destructor_handler,
    uint64_t size,
    uint64_t alignment,
    const ObjectTypeInfo* base_type_info
) : StructTypeInfo(name, size, alignment) 
{
    allocator_handler_ = allocator_handler;
    destructor_handler_ = destructor_handler;
    base_type_info_ = base_type_info;
    name_hash_ = std::hash<std::string>()(name);
    children_type_infos_.push_back(this);   // For UI

    if (strcmp(name_.data(), "Object") == 0) {
        return;
    }

    base_type_info_->update_children(this);
}

bool ObjectTypeInfo::is_a(const ObjectTypeInfo* type_info) const {
    assert(type_info);

    for (const ObjectTypeInfo* it = this; it != nullptr; it = it->base_type_info_)
        if (it->get_name_hash() == type_info->get_name_hash())
            return true;

    return false;
}

bool ObjectTypeInfo::is_exactly(const ObjectTypeInfo* type_info) const {
    assert(type_info);
    return type_info->get_name_hash() == get_name_hash();
}

void ObjectTypeInfo::update_children(const ObjectTypeInfo* new_type_info) const {
    children_type_infos_.push_back(new_type_info);

    if (this != Object::get_static_type_info()) {
        base_type_info_->update_children(new_type_info);
    }
}

void add_member(const StructTypeInfo* type_info, Member* member) {
    assert(type_info);
    assert(member);

    type_info->add_member(member);
}

}