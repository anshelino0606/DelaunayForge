#include "test.h"
#include "type_attribute.h"
#include <iostream>

namespace fem {

FEM_DEFINE_OBJECT(TestObject, Object, Config(), DisplayName("MyTestObject"));

FEM_BEGIN_PROPERTY_REGISTER(TestObject)
{
    FEM_REGISTER_PROPERTY(TestObject, test_);
    FEM_REGISTER_PROPERTY(TestObject, test2_, Binary());
    FEM_REGISTER_PROPERTY(TestObject, test_vec_, ClampMin(1.0f));
}
FEM_END_PROPERTY_REGISTER(TestObject)

FEM_DEFINE_OBJECT(TestObject2, TestObject);
FEM_DEFINE_OBJECT(TestObject3, TestObject2);

FEM_DEFINE_STRUCT(TestStruct);

FEM_BEGIN_PROPERTY_REGISTER(TestStruct)
{
    FEM_REGISTER_PROPERTY(TestStruct, test_vec2);
    FEM_REGISTER_PROPERTY(TestStruct, test_vec3);
    FEM_REGISTER_PROPERTY(TestStruct, test_float);
}
FEM_END_PROPERTY_REGISTER(TestStruct);

FEM_DEFINE_OBJECT(PlaceholderObject, Object);
FEM_BEGIN_PROPERTY_REGISTER(PlaceholderObject)
{
    FEM_REGISTER_PROPERTY(PlaceholderObject, test_value_);
}
FEM_END_PROPERTY_REGISTER(PlaceholderObject);

FEM_DEFINE_STRUCT(PlaceholderStruct, Config(), DisplayName("MyPlaceholder"))
FEM_BEGIN_PROPERTY_REGISTER(PlaceholderStruct)
{
    FEM_REGISTER_PROPERTY(PlaceholderStruct, struct_value1);
    FEM_REGISTER_PROPERTY(PlaceholderStruct, struct_value2);
}
FEM_END_PROPERTY_REGISTER(PlaceholderStruct);

FEM_DEFINE_OBJECT(TestObjectStructObj, Object);
FEM_BEGIN_PROPERTY_REGISTER(TestObjectStructObj)
{
    FEM_REGISTER_PROPERTY(TestObjectStructObj, class_derived_from_object_);
    FEM_REGISTER_PROPERTY(TestObjectStructObj, struct_);
    FEM_REGISTER_PROPERTY(TestObjectStructObj, simple_vec2_);
    FEM_REGISTER_PROPERTY(TestObjectStructObj, enum_);
}
FEM_END_PROPERTY_REGISTER(TestObjectStructObj);

FEM_DEFINE_OBJECT(TestObjectArrays, Object);
FEM_BEGIN_PROPERTY_REGISTER(TestObjectArrays)
{
    FEM_REGISTER_PROPERTY(TestObjectArrays, struct_arr_);
    FEM_REGISTER_PROPERTY(TestObjectArrays, obj_arr_);
    FEM_REGISTER_PROPERTY(TestObjectArrays, vec2_arr_);
    FEM_REGISTER_PROPERTY(TestObjectArrays, int_vec_of_vecs_);
}
FEM_END_PROPERTY_REGISTER(TestObjectArrays)

FEM_DEFINE_OBJECT(TestObjectEnum, Object);
FEM_BEGIN_PROPERTY_REGISTER(TestObjectEnum)
{
    FEM_REGISTER_PROPERTY(TestObjectEnum, test_enum_);
}
FEM_END_PROPERTY_REGISTER(TestObjectEnum)

FEM_DEFINE_ENUM(TestEnum, DisplayName("MyEnum"));

FEM_DEFINE_OBJECT(TestFunctionsObject, Object);
FEM_BEGIN_PROPERTY_REGISTER(TestFunctionsObject)
{
    FEM_REGISTER_FUNCTION(TestFunctionsObject, test);
    FEM_REGISTER_FUNCTION(TestFunctionsObject, test_const);
}
FEM_END_PROPERTY_REGISTER(TestFunctionsObject)

TestObjectStructObj::TestObjectStructObj() {
    class_derived_from_object_ = new PlaceholderObject();
}

TestObjectStructObj::~TestObjectStructObj() {
    delete class_derived_from_object_;
}

void TestFunctionsObject::test() {
    LOG_INFO("Invoking test function!");
}

void TestFunctionsObject::test_const() const {
    LOG_INFO("Invoking const test function!");
}

void parse_properties(Struct* obj, const std::vector<Property*>& properties);

void parse_properties(Object* obj, const std::vector<Property*>& properties) {
    for (Property* property : properties) {
        std::cout << "Property: " << property->display_name() << std::endl;

        switch (property->get_type()) {
            case PropertyType::BOOL: {
                bool value = property->get_value<bool>(obj);
                std::cout << "Value: " << value << std::endl;
                break;
            }
            case PropertyType::INT32: {
                int32_t value = property->get_value<int32_t>(obj);
                std::cout << "Value: " << value << std::endl;;
                property->set_value(obj, value * 2);
                break;
            }
            case PropertyType::FLOAT: {
                float value = property->get_value<float>(obj);
                std::cout << "Value: " << value << std::endl;
                property->set_value(obj, value + 2.66f);
                break;
            }
            case PropertyType::VEC2: {
                glm::vec2 value = property->get_value<glm::vec2>(obj);
                std::cout << "Value: " << value.x << " " << value.y << std::endl;
                break;
            }
            case PropertyType::VEC3: {
                glm::vec3 value = property->get_value<glm::vec3>(obj);
                std::cout << "Value: " << value.x << " " << value.y << " " << value.z << std::endl;
                break;
            }
            case PropertyType::STRUCT: {
                StructContext str_context = property->get_value_as_struct(obj);

                parse_properties(str_context.object, str_context.type_info->get_properties());
                break;
            }
            case PropertyType::OBJECT: {
                Object* prop_obj = property->get_value_as_object(obj);
                parse_properties(prop_obj, prop_obj->get_properties());
                break;
            }
        }

        std::cout << std::endl;
    }
}

void parse_properties(Struct* obj, const std::vector<Property*>& properties) {
    for (Property* property : properties) {
        std::cout << "Property: " << property->display_name() << std::endl;

        switch (property->get_type()) {
            case PropertyType::BOOL: {
                bool value = property->get_value<bool>(obj);
                std::cout << "Value: " << value << std::endl;
                break;
            }
            case PropertyType::INT32: {
                int32_t value = property->get_value<int32_t>(obj);
                std::cout << "Value: " << value << std::endl;;
                property->set_value(obj, value * 2);
                break;
            }
            case PropertyType::FLOAT: {
                float value = property->get_value<float>(obj);
                std::cout << "Value: " << value << std::endl;
                property->set_value(obj, value + 2.66f);
                break;
            }
            case PropertyType::VEC2: {
                glm::vec2 value = property->get_value<glm::vec2>(obj);
                std::cout << "Value: " << value.x << " " << value.y << std::endl;
                break;
            }
            case PropertyType::VEC3: {
                glm::vec3 value = property->get_value<glm::vec3>(obj);
                std::cout << "Value: " << value.x << " " << value.y << " " << value.z << std::endl;
                break;
            }
        }
    }
}

void evaluate_test_object() {
    std::cout << "Starting evaluating object" << std::endl;

    TestObject* obj = create_object<TestObject>();

    std::cout << "Created test object" << std::endl;

    const std::vector<Property*>& properties = obj->get_properties();
    std::cout << "Properties count: " << properties.size() << std::endl;

    parse_properties(obj, properties);

    std::cout << "New test_ value: " << obj->test() << std::endl;
    std::cout << "New test2_ value: " << obj->test2() << std::endl;

    const std::vector<const ObjectTypeInfo*>& children_type_infos = obj->get_children_type_infos();

    std::cout << "TestObject has " << children_type_infos.size() << " children" << std::endl;

    for (const TypeInfo* info : children_type_infos) {
        std::cout << "Child class: " << info->get_name_c_str() << std::endl;
    }

    std::cout << "TestStruct begin" << std::endl;

    TestStruct test_struct;
    test_struct.test_vec2 = glm::vec2(122);
    test_struct.test_vec3 = glm::vec3(135);
    test_struct.test_float = 200.0f;

    parse_properties(&test_struct, test_struct.get_type_info()->get_properties());

    std::cout << "TestStruct end" << std::endl;

    std::cout << "\nTestObjectStructObj begin" << std::endl;

    TestObjectStructObj* new_obj = new TestObjectStructObj();

    parse_properties(new_obj, new_obj->get_properties());

    for (Property* property : new_obj->get_properties()) {
        if (property->get_type() == PropertyType::OBJECT) {
            Object* old_prop_obj = property->get_value_as_object(new_obj);

            parse_properties(old_prop_obj, old_prop_obj->get_properties());

            PlaceholderObject* new_prop_obj_typed = create_object<PlaceholderObject>();
            new_prop_obj_typed->set_value(32);

            property->set_value(new_obj, new_prop_obj_typed);

            Object* new_prop_obj = property->get_value_as_object(new_obj);
            parse_properties(new_prop_obj, new_prop_obj->get_properties());
        }
    }

    std::cout << "TestObjectStructObj end\n" << std::endl;

    std::cout << "TestObjectArrays [Structs] begin" << std::endl;

    TestObjectArrays* obj_arrs = new TestObjectArrays();

    float counter_1 = 0;
    float counter_2 = 0;

    ArrayProperty* arr_struct_property;

    for (Property* property : obj_arrs->get_properties()) {
        if (property->get_type() == PropertyType::ARRAY) {
            arr_struct_property = static_cast<ArrayProperty*>(property);

            for (size_t i = 0; i != 10; ++i) {
                PlaceholderStruct placeholder;
                placeholder.struct_value1 = glm::vec2(counter_1++);
                placeholder.struct_value2 = glm::vec3(counter_2++);
    
                arr_struct_property->add_value(obj_arrs, placeholder);
            }

            break;
        }
    }

    std::vector<size_t> indices_to_erase;

    for (size_t i = 0; i != arr_struct_property->get_element_count(obj_arrs); ++i) {
        StructContext str_context = arr_struct_property->get_value_as_struct(obj_arrs, i);
        parse_properties(str_context.object, str_context.type_info->get_properties());

        if (i % 2 != 0) {
            indices_to_erase.push_back(i);
        }
    }

    size_t offset = 0;

    for (size_t i : indices_to_erase) {
        arr_struct_property->erase(obj_arrs, i - offset);
        offset++;
    }

    std::cout << "TestObjectArrays [Structs] after erasing" << std::endl;

    for (size_t i = 0; i != arr_struct_property->get_element_count(obj_arrs); ++i) {
        StructContext str_context = arr_struct_property->get_value_as_struct(obj_arrs, i);
        parse_properties(str_context.object, str_context.type_info->get_properties());
    }

    std::cout << "TestObjectArrays [Structs] end\n" << std::endl;

    std::cout << "TestObjectArrays [Objects] begin" << std::endl;

    for (Property* property : obj_arrs->get_properties()) {
        if (property->get_type() == PropertyType::ARRAY) {
            arr_struct_property = static_cast<ArrayProperty*>(property);

            if (arr_struct_property->get_value_type() != PropertyType::OBJECT) {
                continue;
            }

            for (int32_t i = 0; i != 10; ++i) {
                PlaceholderObject* placeholder = new PlaceholderObject();
                placeholder->set_value(i);
                arr_struct_property->add_value(obj_arrs, placeholder);
            }

            break;
        }
    }

    PlaceholderObject* temp_placeholder = new PlaceholderObject();
    temp_placeholder->set_value(155);

    arr_struct_property->set_value(obj_arrs, temp_placeholder, 4);
    arr_struct_property->erase(obj_arrs, 1);

    for (size_t i = 0; i != arr_struct_property->get_element_count(obj_arrs); ++i) {
        Object* value = arr_struct_property->get_value_as_object(obj_arrs, i);
        parse_properties(value, value->get_properties());
    }

    std::cout << "TestObjectArrays [Objects] end" << std::endl;

    std::cout << "TestObjectArrays [Vec2] begin" << std::endl;

    for (Property* property : obj_arrs->get_properties()) {
        if (property->get_type() == PropertyType::ARRAY) {
            arr_struct_property = static_cast<ArrayProperty*>(property);

            if (arr_struct_property->get_value_type() != PropertyType::VEC2) {
                continue;
            }

            for (size_t i = 0; i != 10; ++i) {
                arr_struct_property->add_value(obj_arrs, glm::vec2((float)i));
            }

            break;
        }
    }

    for (size_t i = 0; i != arr_struct_property->get_element_count(obj_arrs); ++i) {
        glm::vec2 value = arr_struct_property->get_value<glm::vec2>(obj_arrs, i);
        std::cout << "Vec2 value: " << value.x << " " << value.y << std::endl;
    }

    std::cout << "TestObjectArrays [Vec2] end" << std::endl;

    std::cout << "Test type attributes begin" << std::endl;

    PlaceholderStruct placeholder_struct;

    if (placeholder_struct.has_attribute<Config>()) {
        std::cout << "PlaceholderStruct has Config attr!" << std::endl;
    }

    if (auto display_name = placeholder_struct.get_attribute<DisplayName>()) {
        std::cout << "PlaceholderStruct display name: " << display_name->display_name << std::endl;
    }

    TestObject* test_object = new TestObject();

    if (test_object->has_attribute<Config>()) {
        std::cout << "TestObject has Config attr!" << std::endl;
    }

    if (auto display_name = test_object->get_attribute<DisplayName>()) {
        std::cout << "TestObject display name: " << display_name->display_name << std::endl;
    }

    std::cout << "Test type attributes end" << std::endl;

    std::cout << "Enum test begin" << std::endl;

    TestEnum test_enum = TestEnum::Test1;
    std::cout << "Test enum value 1: " << test_enum.to_string() << std::endl;

    test_enum = "Test3";
    std::cout << "Test enum value 2: " << test_enum.to_string() << std::endl;

    test_enum = 1;
    std::cout << "Test enum value 3: " << test_enum.to_string() << std::endl;

    for (std::string_view value : TestEnum::str_values) {
        std::cout << "Value " << value << std::endl;
    }

    TestEnum test_enum2("Test4");

    const EnumTypeInfo* enum_info = test_enum2.get_type_info();
    enum_info->for_each_element([](std::string_view element) {
        std::cout << "Element from info: " << element << std::endl;
    });

    std::cout << "Enum value from info: " << enum_info->get_value(&test_enum2) << std::endl;

    enum_info->set_value(&test_enum2, "Test3");

    std::cout << "Enum value from info after set: " << enum_info->get_value(&test_enum2) << std::endl;

    std::cout << "Enum test end" << std::endl;

    std::cout << "Object with enum test begin" << std::endl;

    TestObjectEnum* test_obj_enum = new TestObjectEnum();

    test_obj_enum->test_enum_ = TestEnum::Test2;

    EnumContext enum_context;

    for (Property* property : test_obj_enum->get_properties()) {
        if (property->get_type() == PropertyType::ENUM) {
            enum_context = property->get_value_as_enum(test_obj_enum);
        }
    }

    Enum* enum_obj = enum_context.object;
    const EnumTypeInfo* enum_obj_info = enum_context.type_info;
    const DisplayName* enum_name = enum_obj_info->get_attribute<DisplayName>();

    std::cout << "Enum " << enum_name->display_name << " value : " << enum_obj_info->get_value(enum_obj) << std::endl;

    enum_obj_info->set_value(enum_obj, "Test3");
    std::cout << "Enum " << enum_name->display_name << " value after set: " << enum_obj_info->get_value(enum_obj) << std::endl;

    std::cout << "Object with enum test end" << std::endl;

    LOG_INFO("Begin func evaluation test!");

    TestFunctionsObject* function_obj = new TestFunctionsObject();
    for (Function* function : function_obj->get_functions()) {
        function->invoke(function_obj, {});
    }

    LOG_INFO("End func evaluation test!");

    std::cout << "Finished evaluating object" << std::endl;
}

}