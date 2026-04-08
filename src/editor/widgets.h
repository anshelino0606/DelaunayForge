#ifndef FEM_EDITOR_WIDGETS
#define FEM_EDITOR_WIDGETS

#include <imgui/imgui.h>
#include <string_view>

namespace fem {

class Object;
struct Struct;
class TypeInfo;
class Property;
class StructTypeInfo;

template<typename T>
struct ScalarPropertyUITraits {};

class Widgets {
public:
    static void draw_object(Object* object);
    static void draw_struct(Struct* object, const StructTypeInfo* type_info);
    static void execute_post_draw_callbacks();

    template<typename T>
    static bool drag_scalar(
        std::string_view label_name, 
        T& in_out_value, 
        float drag_speed = 1.0f,
        typename ScalarPropertyUITraits<T>::ValueType min_value = 0, 
        typename ScalarPropertyUITraits<T>::ValueType max_value = 0, 
        ImGuiSliderFlags flags = 0, 
        std::string_view format = ""
    ) {
        using Traits = ScalarPropertyUITraits<T>;

        std::string_view user_format = format.empty() ? Traits::format : format;
        
        return ImGui::DragScalarN(
            label_name.data(), 
            Traits::data_type, 
            &in_out_value, 
            Traits::element_count, 
            drag_speed, 
            &min_value, 
            &max_value, 
            user_format.data(), 
            flags
        );
    }

    template<typename T>
    static bool slider_scalar(
        std::string_view label_name, 
        T& in_out_value, 
        typename ScalarPropertyUITraits<T>::ValueType min_value = 0, 
        typename ScalarPropertyUITraits<T>::ValueType max_value = 0, 
        ImGuiSliderFlags flags = 0, 
        std::string_view format = ""
    ) {
        using Traits = ScalarPropertyUITraits<T>;

        std::string_view user_format = format.empty() ? Traits::format : format;

        return ImGui::SliderScalarN(
            label_name.data(), 
            Traits::data_type, 
            &in_out_value, 
            Traits::element_count, 
            &min_value, 
            &max_value, 
            user_format.data(), 
            flags
        );
    }
};

}

#endif // FEM_EDITOR_WIDGETS