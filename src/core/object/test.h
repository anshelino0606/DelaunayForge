#ifndef FEM_OBJECT_TEST_H
#define FEM_OBJECT_TEST_H

#include "object.h"
#include "property.h"

namespace fem {

class TestObject : public Object {
public:
    FEM_DECLARE_OBJECT(TestObject)
    FEM_DECLARE_PROPERTY_REGISTER(TestObject)

public:
    float test() const { return test_; }
    int32_t test2() const { return test2_; }
    glm::vec3 test_vec() const { return test_vec_; }

protected:
    float test_ = 20.0f;
    int32_t test2_ = 25;
    glm::vec3 test_vec_ = glm::vec3(10);
};

class TestObject2 : public TestObject {
    FEM_DECLARE_OBJECT(TestObject2);
};

class TestObject3 : public TestObject2 {
    FEM_DECLARE_OBJECT(TestObject3);
};

struct TestStruct : public Struct {
    FEM_DECLARE_STRUCT(TestStruct);
    FEM_DECLARE_PROPERTY_REGISTER(TestStruct);

    glm::vec2 test_vec2;
    glm::vec3 test_vec3;
    float test_float;
};

struct PlaceholderObject : public Object {
public:
    FEM_DECLARE_OBJECT(PlaceholderObject);
    FEM_DECLARE_PROPERTY_REGISTER(PlaceholderObject);

    int32_t get_value() const {
        return test_value_;
    }

    void set_value(int32_t new_value) {
        test_value_ = new_value;
    }

private:
    int32_t test_value_ = 10;
};

struct PlaceholderStruct : public Struct {
    FEM_DECLARE_STRUCT(PlaceholderStruct);
    FEM_DECLARE_PROPERTY_REGISTER(PlaceholderStruct);

    glm::vec2 struct_value1 = glm::vec2(100);
    glm::vec3 struct_value2 = glm::vec3(200);
};

FEM_DECLARE_ENUM(TestEnum, Test1, Test2, Test3, Test4);

struct TestObjectStructObj : public Object {
public:
    FEM_DECLARE_OBJECT(TestObjectStructObj);
    FEM_DECLARE_PROPERTY_REGISTER(TestObjectStructObj);

    TestObjectStructObj();
    ~TestObjectStructObj();

    bool is_equal(TestObjectStructObj* other) const {
        return other->class_derived_from_object_->get_value() == this->class_derived_from_object_->get_value()
            && other->simple_vec2_ == this->simple_vec2_
            && other->struct_.struct_value1 == this->struct_.struct_value1
            && other->struct_.struct_value2 == this->struct_.struct_value2;
    }

    PlaceholderObject* class_derived_from_object_ = nullptr;
    glm::vec2 simple_vec2_ = glm::vec2(11);
    PlaceholderStruct struct_;
    TestEnum enum_ = TestEnum::Test3;
};

class TestObjectArrays : public Object {
public:
    FEM_DECLARE_OBJECT(TestObjectArrays);
    FEM_DECLARE_PROPERTY_REGISTER(TestObjectArrays);

    std::vector<PlaceholderStruct> struct_arr_;
    std::vector<PlaceholderObject*> obj_arr_;
    std::vector<glm::vec2> vec2_arr_;
    std::vector<std::vector<int>> int_vec_of_vecs_;
};

class TestObjectEnum : public Object {
public:
    FEM_DECLARE_OBJECT(TestObjectEnum);
    FEM_DECLARE_PROPERTY_REGISTER(TestObjectEnum);

    TestEnum test_enum_;
};

class TestFunctionsObject : public Object {
public:
    FEM_DECLARE_OBJECT(TestFunctionsObject);
    FEM_DECLARE_PROPERTY_REGISTER(TestFunctionsObject);

    void test();
    void test_const() const;
};

void evaluate_test_object();

}

#endif // FEM_OBJECT_TEST_H
