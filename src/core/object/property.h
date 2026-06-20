#ifndef FEM_PROPERTY_H
#define FEM_PROPERTY_H

#include "object.h"
#include "function.h"
#include "member.h"
#include "core/utils.h"
#include "property_attribute.h"
#include "core/file_system/archive.h"
#include "logger/logger_macros.h"
#include <glm/glm.hpp>
#include <string>
#include <stdexcept>
#include <utility>

namespace fem {

template<typename T>
using GetPureType = std::remove_pointer_t<T>;

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

template<typename T>
struct get_element_type {
    using type = T;
};

template<typename T, typename Alloc>
struct get_element_type<std::vector<T, Alloc>> {
    using type = T;
};

template<typename T>
using element_type_t = typename get_element_type<std::decay_t<T>>::type;

enum class PropertyType : uint32_t {
    INT32,
    UINT32,
    INT64,
    UINT64,
    FLOAT,
    DOUBLE,
    VEC2,
    VEC3,
    VEC4,
    DVEC2,
    DVEC3,
    DVEC4,
    IVEC2,
    IVEC3,
    IVEC4,
    UVEC2,
    UVEC3,
    UVEC4,
    BOOL,
    MAT3X4,
    MAT4X4,
    QUAT,
    STRING,
    ARRAY,
    OBJECT,
    STRUCT,
    ENUM,
    COUNT
};

constexpr uint32_t property_type_count() {
    return Utils::to_index(PropertyType::COUNT);
}

template<typename T>
struct PropertyTypeEnumMapper {
    constexpr static PropertyType value = 
        std::is_base_of_v<Object, T> ? PropertyType::OBJECT :
        std::is_base_of_v<Struct, T> ? PropertyType::STRUCT :
        PropertyType::BOOL;
};

#define FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(Type, PropertyTypeEnum)            \
    template<>                                                                  \
    struct PropertyTypeEnumMapper<Type>                                         \
    {                                                                           \
        constexpr static PropertyType value = PropertyType::PropertyTypeEnum;   \
    };

FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(bool, BOOL);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(int32_t, INT32);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(uint32_t, UINT32);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(int64_t, INT64);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(uint64_t, UINT64);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(float, FLOAT);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(double, DOUBLE);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::vec2, VEC2);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::vec3, VEC3);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::vec4, VEC4);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::dvec2, DVEC2);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::dvec3, DVEC3);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::dvec4, DVEC4);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::ivec2, IVEC2);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::ivec3, IVEC3);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::ivec4, IVEC4);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::uvec2, UVEC2);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::uvec3, UVEC3);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::uvec4, UVEC4);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::mat3x4, MAT3X4);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::mat4x4, MAT4X4);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(glm::quat, QUAT);
FEM_DEFINE_PROPERTY_TYPE_ENUM_MAPPER(std::string, STRING);

struct StructContext {
    Struct* object = nullptr;
    const StructTypeInfo* type_info = nullptr;
};

struct EnumContext {
    Enum* object = nullptr;
    const EnumTypeInfo* type_info = nullptr;
};

template <typename T>
using EnableIfValidPropertyOwner = typename std::enable_if_t<
    std::is_base_of_v<Object, T> || 
    std::is_base_of_v<Struct, T>,
    int
>;

template<typename T>
struct PropertyTypeTraits {
    using PureType = GetPureType<T>;

    static constexpr bool is_object = std::is_base_of_v<Object, PureType>;
    static constexpr bool is_struct = std::is_base_of_v<Struct, PureType>;
    static constexpr bool is_enum = std::is_base_of_v<Enum, PureType>;
    static constexpr bool is_pointer = std::is_pointer_v<T>;
    static constexpr bool is_vector = is_vector_v<PureType>;
};

class Property : public Member {
public:
    Property(const char* name) : Member(name) { }

    virtual MemberType member_type() const override {
        return MemberType::PROPERTY;
    }

    virtual PropertyType get_type() const = 0;
    virtual uint64_t get_offset() const = 0;
    virtual uint64_t get_size() const = 0;
    virtual const TypeInfo* get_type_info() const = 0;

    template<
        typename ValueType,
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    ValueType& get_value(OwnerType* object) {
        if (get_type() == PropertyType::ARRAY)
            assert(0 && "Can't use set_value method for ArrayProperty.");

        if constexpr (std::is_base_of_v<std::string, ValueType>) {
            std::string* string_data = (std::string*)(((char*)object) + get_offset());
            return *string_data;
        }

        return *static_cast<ValueType*>(get_property_ptr(object));
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void* get_value(OwnerType* object) const {
        if (get_type() == PropertyType::ARRAY)
            assert(0 && "Can't use set_value method for ArrayProperty.");
        return get_property_ptr(object);
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    Object* get_value_as_object(OwnerType* object) const {
        if (get_type() == PropertyType::ARRAY)
            assert(0 && "Can't use get_value method for ArrayProperty.");
        return *reinterpret_cast<Object**>(reinterpret_cast<uint8_t*>(object) + get_offset());
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    StructContext get_value_as_struct(OwnerType* object) const {
        if (get_type() == PropertyType::ARRAY)
            assert(0 && "Can't use get_value method for ArrayProperty.");
        return {
            .object = static_cast<Struct*>(get_property_ptr(object)),
            .type_info = static_cast<const StructTypeInfo*>(get_type_info())
        };
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    EnumContext get_value_as_enum(OwnerType* object) const {
        if (get_type() == PropertyType::ARRAY)
            assert(0 && "Can't use get_value method for ArrayProperty.");
        return {
            .object = static_cast<Enum*>(get_property_ptr(object)),
            .type_info = static_cast<const EnumTypeInfo*>(get_type_info())
        };
    }

    template<
        typename ValueType,
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void set_value(OwnerType* object, const ValueType& value) {
        if (get_type() == PropertyType::ARRAY)
            assert(0 && "Can't use set_value method for ArrayProperty.");
        memcpy(get_property_ptr(object), &value, sizeof(ValueType));
    }

    template<
        typename ValueType,
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void set_value(OwnerType* object, const Object* value) {
        if (get_type() == PropertyType::ARRAY)
            assert(0 && "Can't use set_value method for ArrayProperty.");
        Object* property_old_object = get_value_as_object(object);
        destroy_object(property_old_object);
        value->set_owner(object);
        memcpy(get_property_ptr(object), &value, sizeof(Object*));
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void serialize(OwnerType* object, Archive& archive) {
        serialize_internal(object, archive);
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void deserialize(OwnerType* object, Archive& archive) {
        deserialize_internal(object, archive);
    }

protected:
    void* get_property_ptr(void* object) const {
        assert(object);
        return reinterpret_cast<uint8_t*>(object) + get_offset();
    }

    virtual void serialize_internal(void* owner, Archive& archive) = 0;
    virtual void deserialize_internal(void* owner, Archive& archive) = 0;
};

template<typename ValueType>
class SingleValuePropertyImpl : public Property {
public:
    using Traits = PropertyTypeTraits<ValueType>;
    using ValueTypeNoPtr = Traits::PureType;

    SingleValuePropertyImpl(const char* name) : Property(name) { }

    virtual PropertyType get_type() const override {
        if constexpr (Traits::is_enum) {
            return PropertyType::ENUM;
        } else if constexpr (Traits::is_object) {
            return PropertyType::OBJECT;
        } else if constexpr (Traits::is_struct) {
            return PropertyType::STRUCT;
        } else {
            return PropertyTypeEnumMapper<ValueTypeNoPtr>::value;
        }
    }

    virtual uint64_t get_size() const override {
        return sizeof(ValueTypeNoPtr);
    }

    virtual const TypeInfo* get_type_info() const override {
        if constexpr (Traits::is_object || Traits::is_struct || Traits::is_enum) {
            return ValueTypeNoPtr::get_static_type_info();
        }

        return nullptr;
    }

protected:
    virtual void serialize_internal(void* owner, Archive& archive) override {
        if constexpr (Traits::is_object) {
            Object* value = *reinterpret_cast<Object**>(reinterpret_cast<uint8_t*>(owner) + get_offset());
            value->serialize(archive);
        }
        else if constexpr (Traits::is_struct) {
            Struct* value = static_cast<Struct*>(get_property_ptr(owner));

            ValueTypeNoPtr::get_static_type_info()->for_each_property([value, &archive](Property* property) {
                property->serialize(value, archive);
            });
        } else {
            archive << *static_cast<ValueType*>(get_property_ptr(owner));
        }
    }

    virtual void deserialize_internal(void* owner, Archive& archive) override {
        if constexpr (Traits::is_object) {
            Object* value = create_object(archive);
            value->set_owner(static_cast<Object*>(owner));
            memcpy(get_property_ptr(owner), &value, sizeof(Object*));            
        } else if constexpr (Traits::is_struct) {
            Struct* value = static_cast<Struct*>(get_property_ptr(owner));

            ValueTypeNoPtr::get_static_type_info()->for_each_property([value, &archive](Property* property) {
                property->deserialize(value, archive);
            });
        } else {
            ValueType value;
            archive >> value;
            memcpy(get_property_ptr(owner), &value, sizeof(ValueType));
        }
    }
};

class ArrayProperty : public Property {
public:
    ArrayProperty(const char* name) : Property(name) { }

    virtual PropertyType get_type() const override { return PropertyType::ARRAY; }
    virtual uint64_t get_element_count(Object* object) const { return 0; }
    virtual uint64_t get_element_count(Struct* object) const { return 0; }
    virtual void* get_data(Object* object) { return nullptr; }
    virtual void* get_data(Struct* object) { return nullptr; }
    virtual const void* get_data(Object* object) const { return nullptr; }
    virtual const void* get_data(Struct* object) const { return nullptr; }
    virtual void resize(Object* object, size_t new_size) { }
    virtual void resize(Struct* object, size_t new_size) { }
    virtual const TypeInfo* get_value_type_info() const { return nullptr; }
    virtual PropertyType get_value_type() const = 0;

    template<
        typename ValueType,
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    ValueType& get_value(OwnerType* object, uint64_t index) const {
        return *static_cast<ValueType*>(get_array_value_internal(object, index));
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    uint8_t* get_value_ptr(OwnerType* object, uint64_t index) const {
        return static_cast<uint8_t*>(get_array_value_internal(object, index));
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    StructContext get_value_as_struct(OwnerType* object, uint64_t index) const {
        return {
            .object = static_cast<Struct*>(get_array_value_internal(object, index)),
            .type_info = static_cast<const StructTypeInfo*>(get_value_type_info())
        };
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    Object* get_value_as_object(OwnerType* object, uint64_t index) const {
        return static_cast<Object*>(get_array_value_internal(object, index));
    }

    template<
        typename ValueType,
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    ValueType& back(OwnerType* object) const {
        return get_value<ValueType>(object, get_size() - 1);
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    StructContext back_as_struct(OwnerType* object) const { 
        return get_value_as_struct(object, get_size() - 1);
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    Object* back_as_object(OwnerType* object) const { 
        return get_value_as_object(object, get_size() - 1);
    }

    // Pushes back to array
    template<
        typename OwnerType, 
        typename ValueType,
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void add_value(OwnerType* object, const ValueType& value) {
        if constexpr (std::is_pointer_v<ValueType>) {
            add_array_value_internal(object, value);
        } else {
            add_array_value_internal(object, &value);
        }

        if (const OnArrayValueAdded* on_added = get_attribute<OnArrayValueAdded>()) {
            on_added->on_added(object);
        }
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void emplace_value(OwnerType* object) {
        add_array_value_internal(object, nullptr);

        if (const OnArrayValueAdded* on_added = get_attribute<OnArrayValueAdded>()) {
            on_added->on_added(object);
        }
    }

    template<
        typename OwnerType, 
        typename ValueType,
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void set_value(OwnerType* object, const ValueType& value, uint64_t index) {
        if constexpr (std::is_pointer_v<ValueType>) {
            if (get_value_type() == PropertyType::OBJECT) {
                Object* old_value = get_value_as_object(object, index);
                destroy_object(old_value);
            }

            set_array_value_internal(object, value, index);
        } else {
            set_array_value_internal(object, &value, index);
        }
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void erase(OwnerType* object, uint64_t index) {
        const OnArrayValueRemoved* on_removed_attr = get_attribute<OnArrayValueRemoved>();

        if (on_removed_attr && on_removed_attr->on_pre_removed) {
            on_removed_attr->on_pre_removed(object, get_value_ptr(object, index));
        }

        destroy_object([this, object, index, on_removed_attr] {
            switch (get_value_type()) {
            case PropertyType::OBJECT: {
                Object* old_value = get_value_as_object(object, index);
                destroy_object_immediate(old_value);
                erase_element(object, index);
                break;
            }
            default: {
                erase_element(object, index);
                break;
            }
            }

            if (on_removed_attr && on_removed_attr->on_post_removed) {
                on_removed_attr->on_post_removed(object);
            }
        });
    }

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    void pop_back(OwnerType* object) {
        erase(object, get_size() - 1);
    }

protected:
    virtual void* get_array_value_internal(void* object, uint64_t index) const = 0;
    virtual void add_array_value_internal(void* object, const void* value) = 0;
    virtual void set_array_value_internal(void* object, const void* value, uint64_t index) = 0;
    virtual void erase_element(void* object, uint64_t index) = 0;
};

template<typename ValueType>
class TArrayProperty : public ArrayProperty {
public:
    using ValueTraits = PropertyTypeTraits<ValueType>;
    using ValueTypeNoPtr = ValueTraits::PureType;

    TArrayProperty(const char* name) : ArrayProperty(name) { }

    virtual uint64_t get_size() const override { return sizeof(ValueType); }
    virtual uint64_t get_element_count(Object* object) const override { return get_array(object).size(); }
    virtual uint64_t get_element_count(Struct* object) const override { return get_array(object).size(); }
    virtual void* get_data(Object* object) override { return get_array(object).data(); }
    virtual void* get_data(Struct* object) override { return get_array(object).data(); }
    virtual const void* get_data(Object* object) const override { return get_array(object).data(); }
    virtual const void* get_data(Struct* object) const override { return get_array(object).data(); }
    virtual void resize(Object* object, size_t new_size) override { get_array(object).resize(new_size); }
    virtual void resize(Struct* object, size_t new_size) override { get_array(object).resize(new_size); }
    virtual const TypeInfo* get_type_info() const override { return get_value_type_info(); }

    virtual const TypeInfo* get_value_type_info() const override { 
        if constexpr (ValueTraits::is_object || ValueTraits::is_enum || ValueTraits::is_struct) {
            return ValueTypeNoPtr::get_static_type_info();
        } else {
            return nullptr;
        }
    }

    virtual PropertyType get_value_type() const override {
        if constexpr (ValueTraits::is_enum) {
            return PropertyType::ENUM;
        } else if constexpr (ValueTraits::is_object) {
            return PropertyType::OBJECT;
        } else if constexpr (ValueTraits::is_struct) {
            return PropertyType::STRUCT;
        } else if constexpr (ValueTraits::is_vector) {
            return PropertyType::ARRAY;
        } else {
            return PropertyTypeEnumMapper<ValueTypeNoPtr>::value;
        }
    }

protected:
    virtual void* get_array_value_internal(void* object, uint64_t index) const override {
        // NOTE: avoid std::vector::at() because it throws on OOB.
        if constexpr (ValueTraits::is_pointer) {
            return get_array(object)[index];
        } else {
            return &get_array(object)[index];
        }
    }

    virtual void add_array_value_internal(void* object, const void* value) override {
        if constexpr (ValueTraits::is_pointer) {
            get_array(object).push_back(static_cast<ValueType>(const_cast<void*>(value)));
            Object* typed_value = get_array(object).back();
            typed_value->set_owner(static_cast<Object*>(object));
        } else {
            get_array(object).push_back(*static_cast<const ValueType*>(value));
        }
    }

    virtual void set_array_value_internal(void* object, const void* value, uint64_t index) override {
        std::vector<ValueType>& vec = get_array(object);
        if (index >= vec.size()) {
            LOG_ERROR("ArrayProperty::set_array_value(): Index is %d, array size is %d", index, vec.size());
            return;
        }

        if constexpr (ValueTraits::is_pointer) {
            vec[index] = static_cast<ValueType>(const_cast<void*>(value));
            vec[index]->set_owner(static_cast<Object*>(object));
        } else {
            vec[index] = *static_cast<const ValueType*>(value);
        }
    }

    virtual void erase_element(void* object, uint64_t index) override {
        std::vector<ValueType>& vec = get_array(object);
        if (index >= vec.size()) {
            LOG_ERROR("ArrayProperty::erase_element(): Index is %d, array size is %d", index, vec.size());
            return;
        }

        vec.erase(std::next(vec.begin(), index));    
    }

    virtual void serialize_internal(void* object, Archive& archive) override {
        if constexpr (ValueTraits::is_object || ValueTraits::is_struct) {
            std::vector<ValueType>& vec = get_array(object);
            archive << vec.size();

            for (ValueType& value : vec) {
                if constexpr (ValueTraits::is_object) {
                    value->serialize(archive);
                } else {
                    value.get_type_info()->for_each_property([&value, &archive](Property* property) {
                        property->serialize(&value, archive);
                    });
                }
            }
        } else {
            std::vector<ValueType>& vec = get_array(object);
            archive << vec;
        }
    }

    virtual void deserialize_internal(void* object, Archive& archive) override {
        if constexpr (ValueTraits::is_object || ValueTraits::is_struct) {
            std::vector<ValueType>& vec = get_array(object);

            size_t element_count = 0;
            archive >> element_count;

            constexpr size_t kMaxArrayElements = 10'000'000;
            if (element_count > kMaxArrayElements) {
                LOG_ERROR(
                    "TArrayProperty::deserialize_internal(): suspicious element_count=%zu for property '%s' (cap=%zu). Will deserialize and discard extras.",
                    element_count,
                    raw_name(),
                    kMaxArrayElements
                );
            }

            const size_t keep_count = std::min(element_count, kMaxArrayElements);
            vec.clear();
            vec.resize(keep_count);

            for (size_t i = 0; i != element_count; ++i) {
                if constexpr (ValueTraits::is_object) {
                    ValueTypeNoPtr* obj_value = create_object<ValueTypeNoPtr>(archive);
                    obj_value->set_owner(static_cast<Object*>(object));
                    if (i < keep_count) {
                        vec[i] = obj_value;
                    } else {
                        destroy_object(obj_value);
                    }
                } else {
                    if (i < keep_count) {
                        ValueType& value = vec[i];
                        value.get_type_info()->for_each_property([&value, &archive](Property* property) {
                            property->deserialize(&value, archive);
                        });
                    } else {
                        ValueType tmp{};
                        tmp.get_type_info()->for_each_property([&tmp, &archive](Property* property) {
                            property->deserialize(&tmp, archive);
                        });
                    }
                }
            }
        } else {
            std::vector<ValueType>& vec = get_array(object);
            archive >> vec;
        }
    }   

    std::vector<ValueType>& get_array(void* object) const {
        return *reinterpret_cast<std::vector<ValueType>*>(get_property_ptr(object));
    }
};

template<typename PropertyClass>
Property* allocate_property(const char* name) {
    return new PropertyClass(name);
}

#define FEM_DECLARE_PROPERTY_REGISTER(TypeName) \
    static void register_properties();

#define FEM_BEGIN_PROPERTY_REGISTER(TypeName)   \
    void TypeName::register_properties()

#define FEM_END_PROPERTY_REGISTER(TypeName)     \
    static struct TypeName##PropertyRegistrator \
    {                                           \
        TypeName##PropertyRegistrator()         \
        {                                       \
            TypeName::register_properties();    \
        }                                       \
    } g_##TypeName##PropertyRegistrator;

#define FEM_REGISTER_PROPERTY(TypeName, PropertyName, ...)                                      \
    using PropertyName##RawType = decltype(TypeName::PropertyName);                             \
    using PropertyName##ElemType = element_type_t<PropertyName##RawType>;                       \
    using PropertyName##BaseClass = std::conditional_t<                                         \
            is_vector_v<PropertyName##RawType>,                                                 \
            TArrayProperty<PropertyName##ElemType>,                                             \
            SingleValuePropertyImpl<PropertyName##RawType>                                      \
        >;                                                                                      \
    class PropertyRegistrator_##PropertyName : public PropertyName##BaseClass {                 \
    public:                                                                                     \
        virtual uint64_t get_offset() const override { return offsetof(TypeName, PropertyName); }\
        PropertyRegistrator_##PropertyName(const char* name)                                    \
            : PropertyName##BaseClass(name) { }                                                 \
        virtual bool has_attribute(const char* attrName) const override {                       \
            FEM_DEFINE_HAS_ATTRIBUTE(attrName, __VA_ARGS__);                                    \
        }                                                                                       \
        virtual const void* get_attribute(const char* attrName) const override {                \
            FEM_DEFINE_GET_ATTRIBUTE(attrName, __VA_ARGS__);                                    \
        }                                                                                       \
    };                                                                                          \
    add_member(                                                                                 \
        TypeName::get_static_type_info(),                                                       \
        allocate_property<PropertyRegistrator_##PropertyName>(#PropertyName));

}

#endif // FEM_PROPERTY_H