#ifndef FEM_PROPERTY_ATTRIBUTE_H
#define FEM_PROPERTY_ATTRIBUTE_H

#include "attribute.h"
#include <functional>
#include <variant>
#include <string>
#include <memory>

namespace fem {

using PropertyCallback = void (*)(void* owner);

struct PropertyAttribute {

};

class Property;
class TypeInfo;
class StructTypeInfo;

struct Binary : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(Binary);
};

struct NoUI : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(NoUI);
};

struct Transient : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(Transient)
};

struct Color : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(Color);
};
    
struct EditCondition : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(EditCondition);
    
    enum class Operator {
        Equal,
        NotEqual,
        GreaterThan,
        LessThan,
        And,
        Or
    };
    
    struct Condition {
        std::string property_path;
        Operator op = Operator::Equal;
        
        std::unique_ptr<Condition> left;
        std::unique_ptr<Condition> right;
    };
    
    constexpr EditCondition() = default;
    
    mutable std::unique_ptr<Condition> root_condition;
    
    bool evaluate(const void* owner, const StructTypeInfo* type_info) const;
    
    EditCondition& when_property_equals(const char* property_name, int value) const;
    EditCondition& when_property_equals(const char* property_name, bool value) const;
    EditCondition& when_property_not_equals(const char* property_name, int value) const;
    EditCondition& and_property_equals(const char* property_name, int value) const;
    EditCondition& or_property_equals(const char* property_name, int value) const;
};

struct EditConditionFlags : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(EditConditionFlags);
    
    constexpr EditConditionFlags(const char* prop, uint32_t flags)
        : property_name(prop), allowed_flags(flags) {}
    
    const char* property_name;
    uint32_t allowed_flags;
    
    bool evaluate(const void* owner, const StructTypeInfo* type_info) const;
};

template<typename... Args>
constexpr uint32_t make_enum_flags(Args... values) {
    uint32_t flags = 0;
    ((flags |= (1u << static_cast<int>(values))), ...);
    return flags;
}

#define SHOW_FOR_ENUM(property_name, ...) \
    fem::EditConditionFlags(#property_name, fem::make_enum_flags(__VA_ARGS__))


struct EditConditionMember : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(EditConditionMember);
    
    using Predicate = bool(*)(const void*);
    
    constexpr EditConditionMember(Predicate pred)
        : predicate(pred) {}
    
    Predicate predicate;
    
    bool evaluate(const void* owner) const {
        return predicate(owner);
    }
};

#define MEMBER_CONDITION(OwnerType, member, condition)                                  \
    EditConditionMember(                                                                \
        [](const void* owner) constexpr {                                               \
            const OwnerType* typed_owner = static_cast<const OwnerType*>(owner);        \
            [[maybe_unused]] auto& val = typed_owner->member;                           \
            return condition;                                                           \
        })

#define SHOW_WHEN_MEMBER(OwnerType, member, condition) \
    MEMBER_CONDITION(OwnerType, member, condition)


struct ClampMin : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(ClampMin);
    constexpr ClampMin(float in_min) : min(in_min) { }

    float min = 0.0f;
};

struct ClampMax : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(ClampMax);
    constexpr ClampMax(float in_max) : max(in_max) { }

    float max = 0.0f;
};

struct DragSpeed : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(SliderDragSpeedSpeed);
    constexpr DragSpeed(float in_speed = 1.0f) : speed(in_speed) { }

    float speed = 1.0f;
};

struct Format : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(Format);
    constexpr Format(const char* in_format) : format(in_format) { }

    const char* format;
};

struct DrawCallbacks : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(DrawCallbacks);

    mutable std::function<void(void*)> pre_draw_properties = nullptr;
    mutable std::function<void(void*)> post_draw_properties = nullptr;
};

struct OnValueChanged : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(OnValueChanged);

    mutable std::function<void(void*)> on_value_changed = nullptr;
};

#define ON_VALUE_CHANGED(Type, Callback)                \
    OnValueChanged{                                     \
        .on_value_changed = [](void* obj) {             \
            Type* typed_obj = static_cast<Type*>(obj);  \
            typed_obj->Callback();                      \
        }                                               \
    }

struct Type : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(AssetType);
    constexpr Type(const char* in_type) : type(in_type) { }
    
    const char* type = nullptr;
};

struct SameLine : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(SameLine);
};

// Array properties attributes

struct OnArrayValueAdded : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(OnArrayValueAdded);

    mutable std::function<void(void*)> on_added = nullptr;
};

#define ON_ARRAY_VALUE_ADDED(Type, Callback)            \
    OnArrayValueAdded{                                  \
        .on_added = [](void* obj) {                     \
            Type* typed_obj = static_cast<Type*>(obj);  \
            typed_obj->Callback();                      \
        }                                               \
    }

struct OnArrayValueRemoved : public PropertyAttribute {
    FEM_DEFINE_ATTRIBUTE(OnArrayValueRemoved);

    mutable std::function<void(void* owner, void* elem)> on_pre_removed = nullptr;
    mutable std::function<void(void*)> on_post_removed = nullptr;
};

#define ON_ARRAY_VALUE_PRE_REMOVED(Type, Callback)      \
    OnArrayValueRemoved{                                \
        .on_pre_removed = [](void* obj, void* elem) {   \
            Type* typed_obj = static_cast<Type*>(obj);  \
            typed_obj->Callback(elem);                  \
        }                                               \
    }

#define ON_ARRAY_VALUE_POST_REMOVED(Type, Callback)     \
    OnArrayValueRemoved{                                \
        .on_post_removed = [](void* obj) {              \
            Type* typed_obj = static_cast<Type*>(obj);  \
            typed_obj->Callback();                      \
        }                                               \
    }

#define ON_ARRAY_VALUE_REMOVED(Type, PreCallback, PostCallback)     \
    OnArrayValueRemoved{                                            \
        .on_pre_removed = [](void* obj, void* elem) {               \
            Type* typed_obj = static_cast<Type*>(obj);              \
            typed_obj->PreCallback(elem);                           \
        },                                                          \
        .on_post_removed = [](void* obj) {                          \
            Type* typed_obj = static_cast<Type*>(obj);              \
            typed_obj->PostCallback();                              \
        }                                                           \
    }

}

#endif // FEM_PROPERTY_ATTRIBUTE_H