#include "widget.h"
#include "function_widget.h"
#include "scalar_property_widget.h"
#include "bool_property_widget.h"
#include "string_property_widget.h"
#include "array_property_widget.h"
#include "object_property_widget.h"
#include "struct_property_widget.h"
#include "enum_property_widget.h"
#include "log_categories.h"

namespace fem {

template<typename OwnerType>
bool draw_function_widget(Member* function, OwnerType* owner, const StructTypeInfo* type_info) {
    return FunctionWidget<OwnerType>(static_cast<Function*>(function), owner, type_info).draw();
}

template<typename ScalarType, typename OwnerType>
bool draw_scalar_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    using Widget = ScalarPropertyWidget<ScalarType, OwnerType>;
    return Widget(property, owner, type_info).draw();
}

template<typename OwnerType>
bool draw_bool_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    return BoolPropertyWidget<OwnerType>(property, owner, type_info).draw();
}

template<typename MatType, typename OwnerType>
bool draw_matrix_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    LOGT_ERROR(LogEditor, "Matrices is not supported by UI!");
    return false;
}

template<typename OwnerType>
bool draw_quat_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    LOGT_ERROR(LogEditor, "Quaternions is not supported by UI!");
    return false;
}

template<typename OwnerType>
bool draw_string_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    return StringPropertyWidget<OwnerType>(property, owner, type_info).draw();
}

template<typename OwnerType>
bool draw_array_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    return ArrayPropertyWidget<OwnerType>(static_cast<ArrayProperty*>(property), owner, type_info).draw();
}

template<typename OwnerType>
bool draw_object_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    return ObjectPropertyWidget<OwnerType>(property, owner, type_info).draw();
}

template<typename OwnerType>
bool draw_struct_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    return StructPropertyWidget<OwnerType>(property, owner, type_info).draw();
}

template<typename OwnerType>
bool draw_enum_widget(Property* property, OwnerType* owner, const StructTypeInfo* type_info) {
    return EnumPropertyWidget<OwnerType>(property, owner, type_info).draw();
}

template<typename OwnerType>
using DrawPropertyWidgetHandler = bool(*)(Property*, OwnerType*, const StructTypeInfo*);

#define DEFINE_WIDGET_CALLBACKS(OwnerType)     \
    draw_scalar_widget<int32_t, OwnerType>,    \
    draw_scalar_widget<uint32_t, OwnerType>,   \
    draw_scalar_widget<int64_t, OwnerType>,    \
    draw_scalar_widget<uint64_t, OwnerType>,   \
    draw_scalar_widget<float, OwnerType>,      \
    draw_scalar_widget<double, OwnerType>,     \
    draw_scalar_widget<glm::vec2, OwnerType>,  \
    draw_scalar_widget<glm::vec3, OwnerType>,  \
    draw_scalar_widget<glm::vec4, OwnerType>,  \
    draw_scalar_widget<glm::dvec2, OwnerType>, \
    draw_scalar_widget<glm::dvec3, OwnerType>, \
    draw_scalar_widget<glm::dvec4, OwnerType>, \
    draw_scalar_widget<glm::ivec2, OwnerType>, \
    draw_scalar_widget<glm::ivec3, OwnerType>, \
    draw_scalar_widget<glm::ivec4, OwnerType>, \
    draw_scalar_widget<glm::uvec2, OwnerType>, \
    draw_scalar_widget<glm::uvec3, OwnerType>, \
    draw_scalar_widget<glm::uvec4, OwnerType>, \
    draw_bool_widget<OwnerType>,               \
    draw_matrix_widget<glm::mat3x4, OwnerType>,\
    draw_matrix_widget<glm::mat4x4, OwnerType>,\
    draw_quat_widget<OwnerType>,               \
    draw_string_widget<OwnerType>,             \
    draw_array_widget<OwnerType>,              \
    draw_object_widget<OwnerType>,             \
    draw_struct_widget<OwnerType>,             \
    draw_enum_widget<OwnerType>


DrawPropertyWidgetHandler<Object> g_object_property_widgets[property_type_count()] = {
    DEFINE_WIDGET_CALLBACKS(Object)
};

DrawPropertyWidgetHandler<Struct> g_struct_property_widgets[property_type_count()] = {
    DEFINE_WIDGET_CALLBACKS(Struct)
};

template<typename OwnerType>
bool draw_members(
    OwnerType* object, 
    const StructTypeInfo* type_info, 
    DrawPropertyWidgetHandler<OwnerType>* property_widget_handlers
) {
    const std::vector<Property*>& properties = type_info->get_properties();
    const DrawCallbacks* draw_callbacks = type_info->get_attribute<DrawCallbacks>();

    if (draw_callbacks && draw_callbacks->pre_draw_properties) {
        draw_callbacks->pre_draw_properties(object);
    }

    bool is_owner_object_changed = false;

    for (Member* member : type_info->get_members()) {
        switch(member->member_type()) {
            case MemberType::PROPERTY: {
                Property* property = static_cast<Property*>(member);
                uint32_t property_idx = Utils::to_index(property->get_type());
                is_owner_object_changed |= property_widget_handlers[property_idx](
                    property, object, type_info
                );
                break;
            }
            case MemberType::FUNCTION:{
                is_owner_object_changed |= draw_function_widget(member, object, type_info);
                break;
            }
        }
    }

    return is_owner_object_changed;
}

bool WidgetInternal::draw_members(Object* object, const StructTypeInfo* type_info) {
    return fem::draw_members(object, type_info, g_object_property_widgets);
}

bool WidgetInternal::draw_members(Struct* object, const StructTypeInfo* type_info) {
    return fem::draw_members(object, type_info, g_struct_property_widgets);
}

}