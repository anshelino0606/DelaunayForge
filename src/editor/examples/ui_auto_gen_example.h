#ifndef FEM_UI_AUTO_GEN_EXAMPLE
#define FEM_UI_AUTO_GEN_EXAMPLE

#include "core/object/object.h"
#include "core/object/property.h"

namespace fem {

class UIAutoGenExample_SlidersAndDrags : public Object {
public:
    FEM_DECLARE_OBJECT(UIAutoGenExample_SlidersAndDrags);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_SlidersAndDrags);

    int32_t int32_drag = 1;
    uint32_t uint32_drag = 2;
    int64_t int64_drag = 3;
    uint64_t uint64_drag = 4;
    float float_drag = 55.5f;
    double double_drag = 65.5;
    glm::ivec2 ivec2_drag = glm::ivec2(1);
    glm::ivec3 ivec3_drag = glm::ivec3(1);
    glm::ivec4 ivec4_drag = glm::ivec4(1);
    glm::uvec2 uvec2_drag = glm::uvec2(1);
    glm::uvec3 uvec3_drag = glm::uvec3(1);
    glm::uvec4 uvec4_drag = glm::uvec4(1);
    glm::vec2 vec2_drag = glm::vec2(11.4f);
    glm::vec3 vec3_drag = glm::vec3(12.4f);
    glm::vec4 vec4_drag = glm::vec4(13.4f);
    glm::dvec2 dvec2_drag = glm::dvec2(11.4);
    glm::dvec3 dvec3_drag = glm::dvec3(12.4);
    glm::dvec4 dvec4_drag = glm::dvec4(13.4);

    int32_t int32_slider = 1;
    uint32_t uint32_slider = 2;
    int64_t int64_slider = 3;
    uint64_t uint64_slider = 4;
    float float_slider = 55.5f;
    double double_slider = 65.5;
    glm::ivec2 ivec2_slider = glm::ivec2(1);
    glm::ivec3 ivec3_slider = glm::ivec3(1);
    glm::ivec4 ivec4_slider = glm::ivec4(1);
    glm::uvec2 uvec2_slider = glm::uvec2(1);
    glm::uvec3 uvec3_slider = glm::uvec3(1);
    glm::uvec4 uvec4_slider = glm::uvec4(1);
    glm::vec2 vec2_slider = glm::vec2(11.4f);
    glm::vec3 vec3_slider = glm::vec3(12.4f);
    glm::vec4 vec4_slider = glm::vec4(13.4f);
    glm::dvec2 dvec2_slider = glm::dvec2(11.4);
    glm::dvec3 dvec3_slider = glm::dvec3(12.4);
    glm::dvec4 dvec4_slider = glm::dvec4(13.4);
};

FEM_DECLARE_ENUM(ComboTestEnum, ComboTestValue1, ComboTestValue2, ComboTestValue3);
FEM_DECLARE_ENUM(ToggleTestEnum, ToggleTestValue1, ToggleTestValue2, ToggleTestValue3);

class UIAutoGenExample_StringCheckboxEnum : public Object {
public:
    FEM_DECLARE_OBJECT(UIAutoGenExample_StringCheckboxEnum);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_StringCheckboxEnum);

    std::string string_value_ = "Placeholder";
    bool checkbox_ = false;
    ComboTestEnum combo_test_enum_ = ComboTestEnum::ComboTestValue2;
    ToggleTestEnum toggle_test_enum_ = ToggleTestEnum::ToggleTestValue1;
};

struct UIAutoGenExample_SimpleStruct : public Struct {
    FEM_DECLARE_STRUCT(UIAutoGenExample_SimpleStruct);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_SimpleStruct);

    glm::vec2 value1_ = glm::vec2(15.6f);
    glm::vec3 value2_ = glm::vec3(10.0f);
};

class UIAutoGenExample_ArrayElement : public Object {
public:
    FEM_DECLARE_OBJECT(UIAutoGenExample_ArrayElement);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_ArrayElement);

    UIAutoGenExample_SimpleStruct struct_;
    glm::dvec2 double_vec2_ = glm::dvec2(15.0);
};

class UIAutoGenExample_BaseClass : public Object {
public:
    FEM_DECLARE_OBJECT(UIAutoGenExample_BaseClass);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_BaseClass);

    glm::ivec3 simple_vec_ = glm::ivec3(-10);
};

class UIAutoGenExample_Arrays : public UIAutoGenExample_BaseClass {
public:
    FEM_DECLARE_OBJECT(UIAutoGenExample_Arrays);
    FEM_DECLARE_PROPERTY_REGISTER(TypeUIAutoGenExample_ArraysName);

    std::vector<UIAutoGenExample_ArrayElement*> elements_;
};

struct UIAutoGenExample_StructNoHeader : public Struct {
    FEM_DECLARE_STRUCT(UIAutoGenExample_StructNoHeader);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_StructNoHeader);

    float velocity_ = 10.0f;
    int32_t count_ = 11;
};

class UIAutoGenExample_AnotherChild : public UIAutoGenExample_BaseClass {
public:
    FEM_DECLARE_OBJECT(UIAutoGenExample_AnotherChild);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_AnotherChild);

    float float_test_ = 15.5f;
    UIAutoGenExample_StructNoHeader another_struct_;
};

class UIAutoGenExample_BaseClassContainer : public Object {
public:
    FEM_DECLARE_OBJECT(UIAutoGenExample_BaseClassContainer);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_BaseClassContainer);

    UIAutoGenExample_BaseClassContainer() {
        test_base_object_ = create_object<UIAutoGenExample_BaseClass>();
    }

    ~UIAutoGenExample_BaseClassContainer() {
        destroy_object(test_base_object_);
    }

    UIAutoGenExample_BaseClass* test_base_object_;
};

class UIAutoGenExample_Functions : public Object {
public:
    FEM_DECLARE_OBJECT(UIAutoGenExample_Functions);
    FEM_DECLARE_PROPERTY_REGISTER(UIAutoGenExample_Functions);

    void func_1();
    void func_2() const;
};


void draw_test_auto_ui_gen_window();

}

#endif // FEM_UI_AUTO_GEN_EXAMPLE