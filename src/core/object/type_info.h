#ifndef FEM_TYPE_INFO_H
#define FEM_TYPE_INFO_H

#include <string>
#include <functional>

namespace fem {

class Property;
class Function;
class Member;
class Object;
struct Enum;
class TypeInfo;
class TypeManager;
class TypeAttribute;

class StructTypeInfo;

void add_member(const StructTypeInfo* type_info, Member* member);

enum class ReflectedObjectType {
    OBJECT,
    STRUCT,
    ENUM
};

class TypeInfo {
public:
    friend TypeManager;

    TypeInfo(
        const char* name,
        uint64_t size,
        uint64_t alignment
    );

    virtual ~TypeInfo() = default;

    std::string get_name() const { return std::string(name_); }
    const char* get_name_c_str() const { return name_.data(); };
    uint64_t get_name_hash() const { return name_hash_; }
    
    uint64_t get_class_size() const { return class_size_; }
    uint64_t get_class_alignment() const { return class_alignment_; }

    virtual bool has_attribute(const char* attribute_name) const { return false; };
    virtual const void* get_attribute(const char* attribute_name) const { return nullptr; }

    template<typename AttributeType>
    bool has_attribute() const {
        return has_attribute(AttributeType::name);
    }

    template<typename AttributeType>
    const AttributeType* get_attribute() const { 
        return static_cast<const AttributeType*>(get_attribute(AttributeType::name));
    }

    virtual ReflectedObjectType type() const = 0;

protected:
    std::string_view name_;
    uint64_t class_size_;
    uint64_t class_alignment_;
    uint64_t name_hash_;

    virtual void cleanup() const { };
};

class EnumTypeInfo : public TypeInfo {
public:
    using TypeInfo::TypeInfo;

    virtual void for_each_element(const std::function<void(std::string_view)>& callback) const = 0;
    virtual void set_value(Enum* object, std::string_view value) const = 0;
    virtual std::string_view get_value(Enum* object) const = 0;

    virtual ReflectedObjectType type() const override { return ReflectedObjectType::ENUM; }
};

class StructTypeInfo : public TypeInfo {
public:
    friend TypeManager;
    friend void add_member(const StructTypeInfo* type_info, Member* member);

    using ForEachPropertyHandler = std::function<void(Property*)>;
    using ForEachFunctionHandler = std::function<void(Function*)>;

    using TypeInfo::TypeInfo;

    const std::vector<Member*>& get_members() const { 
        return members_;
    }

    // For some tests, must not be used in the main app
    std::vector<Property*> get_properties() const;

    // For some tests, must not be used in the main app
    std::vector<Function*> get_functions() const;

    Property* get_property(std::string_view property_name) const;
    Function* get_function(std::string_view function_name) const;
    Member* get_member(std::string_view member_name) const;

    virtual ReflectedObjectType type() const override { return ReflectedObjectType::STRUCT; }

    void for_each_property(const ForEachPropertyHandler& callback) const;
    void for_each_function(const ForEachFunctionHandler& callback) const;

protected:
    mutable std::vector<Member*> members_;

    void add_member(Member* member) const;
    virtual void cleanup() const override;
};

class ObjectTypeInfo : public StructTypeInfo {
public:
    using AllocatorHandler = std::function<Object*()>;
    using DestructorHandler = std::function<void(Object*)>;

    friend TypeManager;
    friend void add_member(const StructTypeInfo* type_info, Member* member);

    ObjectTypeInfo(
        const char* name,
        AllocatorHandler allocator_handler,
        DestructorHandler destructor_handler,
        uint64_t size,
        uint64_t alignment,
        const ObjectTypeInfo* base_type_info
    );

    AllocatorHandler get_allocator_handler() const { return allocator_handler_; }
    DestructorHandler get_destructor_handler() const { return destructor_handler_; }
    const ObjectTypeInfo* get_base_type_info() const { return base_type_info_; }

    const std::vector<const ObjectTypeInfo*>& get_children_type_infos() const {
        return children_type_infos_;
    }

    bool is_a(const ObjectTypeInfo* type_info) const;
    bool is_exactly(const ObjectTypeInfo* type_info) const;

    template<typename T>
    bool is_a() const {
        return is_a(T::get_static_type_info());
    }

    template<typename T>
    bool is_exactly() const {
        return is_exactly(T::get_static_type_info());
    }

    virtual ReflectedObjectType type() const override { return ReflectedObjectType::OBJECT; }    

protected:
    AllocatorHandler allocator_handler_;
    DestructorHandler destructor_handler_;

    const ObjectTypeInfo* base_type_info_ = nullptr;
    mutable std::vector<const ObjectTypeInfo*> children_type_infos_;

    void update_children(const ObjectTypeInfo* new_type_info) const;
};

}

#endif // FEM_TYPE_INFO_H