#ifndef FEM_TYPE_MANAGER_H
#define FEM_TYPE_MANAGER_H

#include <vector>
#include <string>
#include <unordered_map>

namespace fem {

class Object;
class TypeInfo;
class ObjectTypeInfo;

class TypeManager {
public:
    static void cleanup();

    static const TypeInfo* get_type_info(const char* type_name);
    
    static Object* create_object(const ObjectTypeInfo* type_info);
    static Object* create_object_by_name(const char* type_name);
    static void destroy_object(Object* object);

    static void register_type(const TypeInfo* type_info);

private:
    inline static std::unordered_map<std::string, uint32_t> s_index_by_type_name_{};
    inline static std::vector<const TypeInfo*> s_type_infos_{};
};

}

#endif // FEM_TYPE_MANAGER_H