#include "ui_auto_gen_example.h"
#include "editor/widgets/object_widget.h"
#include "core/object/type_attribute.h"

namespace fem {

FEM_DEFINE_OBJECT(UIAutoGenExample_SlidersAndDrags, Object, DisplayName("Drags and Sliders"));
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_SlidersAndDrags)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, int32_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uint32_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, int64_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uint64_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, float_drag, ClampMin(0.0f));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, double_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, ivec2_drag, DisplayName("Another CustomName"));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, ivec3_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, ivec4_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uvec2_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uvec3_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uvec4_drag, DragSpeed(5.0f), ClampMax(100));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, vec2_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, vec3_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, vec4_drag, DisplayName("Vec4CustomName!!!"));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, dvec2_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, dvec3_drag);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, dvec4_drag, DragSpeed(0.05f), ClampMin(2.0f), ClampMax(150.0f));

    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, int32_slider, ClampMin(-100), ClampMax(100), DisplayName("Int32 CustomName!"));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uint32_slider, ClampMin(3), ClampMax(15));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, int64_slider, ClampMin(-200), ClampMax(200));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uint64_slider, ClampMin(1), ClampMax(1000));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, float_slider, ClampMin(2.0f), ClampMax(15.676f));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, double_slider, ClampMin(-50.0f), ClampMax(50.0f));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, ivec2_slider, ClampMin(-20), ClampMax(20));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, ivec3_slider, ClampMin(-30), ClampMax(30));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, ivec4_slider, ClampMin(-40), ClampMax(40));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uvec2_slider, ClampMin(20), ClampMax(200));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uvec3_slider, ClampMin(30), ClampMax(300));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, uvec4_slider, ClampMin(40), ClampMax(400));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, vec2_slider, ClampMin(1.5f), ClampMax(6.5f));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, vec3_slider, ClampMin(35.0f), ClampMax(255.4f));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, vec4_slider, ClampMin(12.0f), ClampMax(188.5f));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, dvec2_slider, ClampMin(-256.66), ClampMax(-32.5));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, dvec3_slider, ClampMin(-1000.0), ClampMax(1000.0));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SlidersAndDrags, dvec4_slider, ClampMin(2000.0), ClampMax(5000));
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_SlidersAndDrags);

FEM_DEFINE_ENUM(ComboTestEnum);
FEM_DEFINE_ENUM(ToggleTestEnum, DrawAsToggles());

FEM_DEFINE_OBJECT(UIAutoGenExample_StringCheckboxEnum, Object, DisplayName("String, Checkbox and Enums"));
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_StringCheckboxEnum)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_StringCheckboxEnum, string_value_);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_StringCheckboxEnum, checkbox_);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_StringCheckboxEnum, toggle_test_enum_);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_StringCheckboxEnum, combo_test_enum_);
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_StringCheckboxEnum)

FEM_DEFINE_STRUCT(UIAutoGenExample_SimpleStruct, DisplayName("SimpleStruct"));
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_SimpleStruct)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SimpleStruct, value1_, DisplayName("StructValue1"));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_SimpleStruct, value2_, DisplayName("StructValue2"));
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_SimpleStruct)

FEM_DEFINE_OBJECT(UIAutoGenExample_ArrayElement, Object, DisplayName("CustomArrayElement"));
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_ArrayElement)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_ArrayElement, struct_, DisplayName("StructWithValues"));
    FEM_REGISTER_PROPERTY(UIAutoGenExample_ArrayElement, double_vec2_);
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_ArrayElement);

FEM_DEFINE_OBJECT(UIAutoGenExample_BaseClass, Object, DisplayName("TestBaseClass"));
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_BaseClass)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_BaseClass, simple_vec_);
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_BaseClass)

FEM_DEFINE_STRUCT(UIAutoGenExample_StructNoHeader, DisplayName("StructNoHeader"));
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_StructNoHeader)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_StructNoHeader, velocity_);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_StructNoHeader, count_);
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_StructNoHeader)

FEM_DEFINE_OBJECT(UIAutoGenExample_AnotherChild, UIAutoGenExample_BaseClass, DisplayName("TestAnotherChild"));
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_AnotherChild)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_AnotherChild, float_test_);
    FEM_REGISTER_PROPERTY(UIAutoGenExample_AnotherChild, another_struct_, NoTypeHeader());
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_AnotherChild)

FEM_DEFINE_OBJECT(UIAutoGenExample_Arrays, UIAutoGenExample_BaseClass, DisplayName("ExampleArrays"));
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_Arrays)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_Arrays, elements_);
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_Arrays)

FEM_DEFINE_OBJECT(UIAutoGenExample_BaseClassContainer, Object, NoTypeHeader());
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_BaseClassContainer)
{
    FEM_REGISTER_PROPERTY(UIAutoGenExample_BaseClassContainer, test_base_object_, BaseClass());
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_BaseClassContainer)

FEM_DEFINE_OBJECT(UIAutoGenExample_Functions, Object);
FEM_BEGIN_PROPERTY_REGISTER(UIAutoGenExample_Functions)
{
    FEM_REGISTER_FUNCTION(UIAutoGenExample_Functions, func_1);
    FEM_REGISTER_FUNCTION(UIAutoGenExample_Functions, func_2, DisplayName("Press Me :)"));
}
FEM_END_PROPERTY_REGISTER(UIAutoGenExample_Functions)

void UIAutoGenExample_Functions::func_1() {
    LOG_INFO("Func 1 is called!");
}

void UIAutoGenExample_Functions::func_2() const {
    LOG_INFO("Func 2 is called");
}

UIAutoGenExample_SlidersAndDrags* g_slider_and_drags_example = nullptr;
UIAutoGenExample_Arrays* g_arrays_example = nullptr;
UIAutoGenExample_StringCheckboxEnum* g_string_checkbox_enum_example = nullptr;
UIAutoGenExample_BaseClassContainer* g_base_class_example = nullptr;
UIAutoGenExample_Functions* g_functions_example = nullptr;

void draw_test_auto_ui_gen_window() {
    if (!g_slider_and_drags_example) {
        g_slider_and_drags_example = create_object<UIAutoGenExample_SlidersAndDrags>();
    }

    if (!g_arrays_example) {
        g_arrays_example = create_object<UIAutoGenExample_Arrays>();
    }

    if (!g_string_checkbox_enum_example) {
        g_string_checkbox_enum_example = create_object<UIAutoGenExample_StringCheckboxEnum>();
    }

    if (!g_base_class_example) {
        g_base_class_example = create_object<UIAutoGenExample_BaseClassContainer>();
    }

    if (!g_functions_example) {
        g_functions_example = create_object<UIAutoGenExample_Functions>();
    }

    ImGui::Begin("Test Auto UI Slider and Drags Generation");
    ObjectWidget().draw(g_slider_and_drags_example);
    ImGui::End();

    ImGui::Begin("Test Auto UI String Checkbox Enum Generation");
    ObjectWidget().draw(g_string_checkbox_enum_example);
    ImGui::End();

    ImGui::Begin("Test Auto UI Array Generation");
    ObjectWidget().draw(g_arrays_example);
    ImGui::End();

    ImGui::Begin("Test Auto UI Base Class Generation");
    ObjectWidget().draw(g_base_class_example);
    ImGui::End();

    ImGui::Begin("Test Auto UI Functions Generation");
    ObjectWidget().draw(g_functions_example);
    ImGui::End();
}

}