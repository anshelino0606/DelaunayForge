#ifndef FEM_OBJECT_H
#define FEM_OBJECT_H

#include "type_info.h"
#include "type_manager.h"
#include "attribute.h"
#include "type_attribute.h"
#include "object_destruction_queue.h"
#include "core/macro.h"
#include "core/compile_time_hash.h"
#include "core/file_system/archive.h"

#include <array>

namespace fem {

class Object;
class Property;
class Function;
class Archive;

#define FEM_DECLARE_CUSTOM_TYPE_INFO(TypeName, TypeInfoClass)                           \
    class TypeInfo_##TypeName : public TypeInfoClass {                                  \
    public:                                                                             \
        using TypeInfoClass::TypeInfoClass;                                             \
        virtual bool has_attribute(const char* attribute_name) const override;          \
        virtual const void* get_attribute(const char* attribute_name) const override;   \
    };

#define FEM_DEFINE_CUSTOM_TYPE_INFO(TypeName, ...)  \
    bool TypeName::TypeInfo_##TypeName::has_attribute(const char* attribute_name) const {           \
        FEM_DEFINE_HAS_ATTRIBUTE(attribute_name, __VA_ARGS__);                                      \
    }                                                                                               \
    const void* TypeName::TypeInfo_##TypeName::get_attribute(const char* attribute_name) const {    \
        FEM_DEFINE_GET_ATTRIBUTE(attribute_name, __VA_ARGS__);                                      \
    }

#define FEM_DECLARE_OBJECT(TypeName)                                                                            \
    public:                                                                                                     \
        FEM_DECLARE_CUSTOM_TYPE_INFO(TypeName, ObjectTypeInfo)                                                  \
        static Object* allocate() { return new TypeName(); }                                                    \
        static void destroy(Object* ptr) { delete ptr; }                                                        \
        static const ObjectTypeInfo* get_static_type_info();                                                    \
        virtual const ObjectTypeInfo* get_type_info() const { return get_static_type_info(); }

#define FEM_DEFINE_ROOT_OBJECT(TypeName)                        \
    FEM_DEFINE_CUSTOM_TYPE_INFO(TypeName)                       \
    const ObjectTypeInfo* TypeName::get_static_type_info() {    \
        static TypeInfo_##TypeName type_info(                   \
            #TypeName,                                          \
            TypeName::allocate,                                 \
            TypeName::destroy,                                  \
            sizeof(TypeName),                                   \
            alignof(TypeName),                                  \
            nullptr                                             \
        );                                                      \
        return &type_info;                                      \
    }

#define FEM_DEFINE_OBJECT(TypeName, BaseTypeName, ...)          \
    FEM_DEFINE_CUSTOM_TYPE_INFO(TypeName, __VA_ARGS__)          \
    const ObjectTypeInfo* TypeName::get_static_type_info() {    \
        static TypeInfo_##TypeName type_info(                   \
            #TypeName,                                          \
            TypeName::allocate,                                 \
            TypeName::destroy,                                  \
            sizeof(TypeName),                                   \
            alignof(TypeName),                                  \
            BaseTypeName::get_static_type_info()                \
        );                                                      \
        return &type_info;                                      \
    }                                                           \
    static struct TypeName##Registrator                         \
    {                                                           \
        TypeName##Registrator()                                 \
        {                                                       \
            TypeManager::register_type(                         \
                TypeName::get_static_type_info());              \
        }                                                       \
    } g_##TypeName##Registrator;

#define FEM_DECLARE_STRUCT(TypeName)                            \
    public:                                                     \
        FEM_DECLARE_CUSTOM_TYPE_INFO(TypeName, StructTypeInfo); \
        static const TypeInfo_##TypeName s_type_info;           \
        static const StructTypeInfo* get_static_type_info() {   \
            return &TypeName::s_type_info;                      \
        }                                                       \
        const StructTypeInfo* get_type_info() const {           \
            return &TypeName::s_type_info;                      \
        }                                                       \
        const std::vector<Member*>& get_members() const {       \
            return get_type_info()->get_members();              \
        }                                                       \
        template<typename AttrType>                             \
        bool has_attribute() const {                            \
            return get_type_info()->has_attribute<AttrType>();  \
        }                                                       \
        template<typename AttrType>                             \
        const AttrType* get_attribute() const {                 \
            return get_type_info()->get_attribute<AttrType>();  \
        }

#define FEM_DEFINE_STRUCT(TypeName, ...)                        \
    FEM_DEFINE_CUSTOM_TYPE_INFO(TypeName, __VA_ARGS__);         \
    const TypeName::TypeInfo_##TypeName TypeName::s_type_info(  \
        #TypeName,                                              \
        sizeof(TypeName),                                       \
        alignof(TypeName)                                       \
    );                                                          \
    static struct TypeName##Registrator                         \
    {                                                           \
        TypeName##Registrator()                                 \
        {                                                       \
            TypeManager::register_type(                         \
                TypeName::get_static_type_info());              \
        }                                                       \
    } g_##TypeName##Registrator;

struct Enum {};

#define FEM_DECLARE_ENUM_ELEMENT(ElementName) ElementName,
#define FEM_ENUM_ELEMENT_STR_NAME(ElementName) #ElementName,

#define FEM_ENUM_ELEMENT_TYPE_HASH(ElementName) compile_time_fnv1(#ElementName)
#define FEM_ENUM_FROM_STRING(ElementName) case compile_time_fnv1(#ElementName): return ElementName;

#define FEM_DECLARE_ENUM(EnumName, ...)                                                 \
    class EnumName##TypeInfo : public EnumTypeInfo {                                    \
    public:                                                                             \
        using EnumTypeInfo::EnumTypeInfo;                                               \
        virtual bool has_attribute(const char* attribute_name) const override;          \
        virtual const void* get_attribute(const char* attribute_name) const override;   \
        virtual void for_each_element(                                                  \
            const std::function<void(std::string_view)>& callback) const override;      \
        virtual void set_value(Enum* object, std::string_view value) const override;    \
        virtual std::string_view get_value(Enum* object) const override;                \
    };                                                                                  \
    struct EnumName : public Enum {                                                     \
        static const EnumName##TypeInfo s_type_info;                                    \
        static const EnumTypeInfo* get_static_type_info() {                             \
            return &EnumName::s_type_info;                                              \
        }                                                                               \
        const EnumTypeInfo* get_type_info() const {                                     \
            return &EnumName::s_type_info;                                              \
        }                                                                               \
        template<typename AttrType>                                                     \
        bool has_attribute() const {                                                    \
            return get_type_info()->has_attribute<AttrType>();                          \
        }                                                                               \
        template<typename AttrType>                                                     \
        const AttrType* get_attribute() const {                                         \
            return get_type_info()->get_attribute<AttrType>();                          \
        }                                                                               \
        enum Type : uint32_t {                                                          \
            FEM_FOR_EACH(FEM_DECLARE_ENUM_ELEMENT, __VA_ARGS__)                         \
        };                                                                              \
        Type value;                                                                     \
        static constexpr size_t element_count = FEM_NARG(__VA_ARGS__);                  \
        static constexpr std::array<std::string_view, element_count> str_values = {     \
            FEM_FOR_EACH(FEM_ENUM_ELEMENT_STR_NAME, __VA_ARGS__)                        \
        };                                                                              \
        constexpr EnumName() : value((Type)0) {}                                        \
        constexpr EnumName(uint32_t initial_value) : value((Type)initial_value) {}      \
        constexpr EnumName(Type initial_value) : value(initial_value) {}                \
        EnumName(std::string_view initial_value) {                                      \
            from_string(initial_value);                                                 \
        }                                                                               \
        void operator=(uint32_t initial_value) {                                        \
            value = (Type)initial_value;                                                \
        }                                                                               \
        void operator=(std::string_view initial_value) {                                \
            from_string(initial_value);                                                 \
        }                                                                               \
        bool operator==(const EnumName::Type& other) const {                            \
            return other == this->value;                                                \
        }                                                                               \
        static std::string_view to_string(EnumName::Type in_value) {                    \
            return str_values[in_value];                                                \
        }                                                                               \
        static EnumName::Type to_enum(std::string_view in_value) {                      \
            switch (runtime_fnv1(in_value)) {                                           \
                FEM_FOR_EACH(FEM_ENUM_FROM_STRING, __VA_ARGS__)                         \
                default: return (EnumName::Type)0;                                      \
            };                                                                          \
        }                                                                               \
        std::string_view to_string() const {                                            \
            return str_values[value];                                                   \
        }                                                                               \
        void from_string(std::string_view str_value) {                                  \
            value = to_enum(str_value);                                                 \
        }                                                                               \
        operator uint32_t() const {                                                     \
            return value;                                                               \
        }                                                                               \
        operator uint32_t&() {                                                          \
            return (uint32_t&)value;                                                    \
        }                                                                               \
        operator const uint32_t&() const {                                              \
            return (uint32_t&)value;                                                    \
        }                                                                               \
    };

#define FEM_DEFINE_ENUM(EnumName, ...)                                                  \
    const EnumName##TypeInfo EnumName::s_type_info(                                     \
        #EnumName,                                                                      \
        sizeof(EnumName),                                                               \
        alignof(EnumName)                                                               \
    );                                                                                  \
    static struct EnumName##Registrator                                                 \
    {                                                                                   \
        EnumName##Registrator()                                                         \
        {                                                                               \
            TypeManager::register_type(                                                 \
                EnumName::get_static_type_info());                                      \
        }                                                                               \
    } g_##EnumName##Registrator;                                                        \
    void EnumName##TypeInfo::for_each_element(                                          \
        const std::function<void(std::string_view)>& callback                           \
    ) const {                                                                           \
        for (std::string_view element : EnumName::str_values) {                         \
            callback(element);                                                          \
        }                                                                               \
    }                                                                                   \
    void EnumName##TypeInfo::set_value(                                                 \
        Enum* object, std::string_view value                                            \
    ) const {                                                                           \
        EnumName& typed_obj = *static_cast<EnumName*>(object);                          \
        typed_obj.from_string(value);                                                   \
    }                                                                                   \
    std::string_view EnumName##TypeInfo::get_value(Enum* object) const {                \
        return static_cast<EnumName*>(object)->to_string();                             \
    }                                                                                   \
    bool EnumName##TypeInfo::has_attribute(const char* attribute_name) const {          \
        FEM_DEFINE_HAS_ATTRIBUTE(attribute_name, __VA_ARGS__);                          \
    }                                                                                   \
    const void* EnumName##TypeInfo::get_attribute(const char* attribute_name) const {   \
        FEM_DEFINE_GET_ATTRIBUTE(attribute_name, __VA_ARGS__);                          \
    }

struct Struct {};

class Object {
    FEM_DECLARE_OBJECT(Object)

public:
    Object() = default;
    virtual ~Object() = default;

    bool is_exactly(const ObjectTypeInfo* class_type_info) const {
        return get_type_info()->is_exactly(class_type_info);
    }

    bool is_a(const ObjectTypeInfo* class_type_info) const {
        return get_type_info()->is_a(class_type_info);
    }

    template<typename T>
    bool is_exactly() const {
        return is_exactly(T::get_static_type_info());
    }

    template<typename T>
    bool is_a() const {
        return is_a(T::get_static_type_info());
    }

    // Only for some tests, must not be used in the main app
    std::vector<Property*> get_properties() const {
        return get_type_info()->get_properties();
    }

    // Only for some tests, must not be used in the main app
    std::vector<Function*> get_functions() const { 
        return get_type_info()->get_functions();
    }

    const std::vector<Member*>& get_members() const { 
        return get_type_info()->get_members();
    }

    const std::vector<const ObjectTypeInfo*>& get_children_type_infos() const {
        return get_type_info()->get_children_type_infos();
    }

    template<typename AttributeType>
    bool has_attribute() const {
        return get_type_info()->has_attribute<AttributeType>();
    }

    template<typename AttributeType>
    const AttributeType* get_attribute() const {
        return get_type_info()->get_attribute<AttributeType>();
    }

    virtual void serialize(Archive& archive);
    virtual void deserialize(Archive& archive);
    virtual void serialize();

    virtual void set_owner(Object* object) {
        owner_ = object;
    }

    Object* get_owner() const { 
        return owner_; 
    }

protected:
    std::string last_path_;

    Object* owner_ = nullptr;

    template<typename T>
    T* create_subobject() {
        T* object = static_cast<T*>(T::allocate());
        object->set_owner(this);
        return object;
    }
};

template<typename T>
T* create_object() {
    return static_cast<T*>(T::allocate());
}

inline Object* create_object(const ObjectTypeInfo* type_info) {
    return type_info->get_allocator_handler()();
}

inline Object* create_object(const std::string& name) {
    return TypeManager::create_object_by_name(name.c_str());
}

inline Object* create_object(Archive& archive) {
    std::string type_name;
    archive >> type_name;
    Object* object = create_object(type_name);
    object->deserialize(archive);
    return object;
}

template<typename T>
T* create_object(Archive& archive) {
    return static_cast<T*>(create_object(archive));
}

inline ObjectDestructionQueue g_object_destruction_queue;

inline void destroy_object_immediate(Object* object) {
    TypeManager::destroy_object(object);
}

inline void destroy_object(Object* object) {
    g_object_destruction_queue.destroy(object);
}

inline void destroy_object(const ObjectDestroyHandler& handler) {
    g_object_destruction_queue.destroy(handler);
}

}

#endif // FEM_OBJECT_H