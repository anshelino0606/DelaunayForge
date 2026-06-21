#include "widget_internal.h"
#include "function_widget.h"
#include "scalar_property_widget.h"
#include "bool_property_widget.h"
#include "string_property_widget.h"
#include "matrix_property_widget.h"
#include "quat_property_widget.h"
#include "array_property_widget.h"
#include "object_property_widget.h"
#include "struct_property_widget.h"
#include "enum_property_widget.h"

namespace fem {

template<typename OwnerType>
bool draw_function_widget(Member* function, OwnerType* owner, const StructTypeInfo* type_info) {
    return FunctionWidget<OwnerType>(static_cast<Function*>(function), owner, type_info).draw();
}

template<typename WidgetType, typename PropertyCastType = Property>
bool draw_property_widget(Property* property, typename WidgetType::OwnerType* owner, const StructTypeInfo* type_info) {
    if constexpr (std::is_same_v<PropertyCastType, Property>)
        return WidgetType(property, owner, type_info).draw();
    else
        return WidgetType(static_cast<PropertyCastType*>(property), owner, type_info).draw();
}

template<typename OwnerType>
using DrawPropertyWidgetHandler = bool(*)(Property*, OwnerType*, const StructTypeInfo*);

#define DEFINE_WIDGET_CALLBACKS(OwnerType)                                   \
    draw_property_widget<ScalarPropertyWidget<int32_t, OwnerType>>,          \
    draw_property_widget<ScalarPropertyWidget<uint32_t, OwnerType>>,         \
    draw_property_widget<ScalarPropertyWidget<int64_t, OwnerType>>,          \
    draw_property_widget<ScalarPropertyWidget<uint64_t, OwnerType>>,         \
    draw_property_widget<ScalarPropertyWidget<float, OwnerType>>,            \
    draw_property_widget<ScalarPropertyWidget<double, OwnerType>>,           \
    draw_property_widget<ScalarPropertyWidget<glm::vec2, OwnerType>>,        \
    draw_property_widget<ScalarPropertyWidget<glm::vec3, OwnerType>>,        \
    draw_property_widget<ScalarPropertyWidget<glm::vec4, OwnerType>>,        \
    draw_property_widget<ScalarPropertyWidget<glm::dvec2, OwnerType>>,       \
    draw_property_widget<ScalarPropertyWidget<glm::dvec3, OwnerType>>,       \
    draw_property_widget<ScalarPropertyWidget<glm::dvec4, OwnerType>>,       \
    draw_property_widget<ScalarPropertyWidget<glm::ivec2, OwnerType>>,       \
    draw_property_widget<ScalarPropertyWidget<glm::ivec3, OwnerType>>,       \
    draw_property_widget<ScalarPropertyWidget<glm::ivec4, OwnerType>>,       \
    draw_property_widget<ScalarPropertyWidget<glm::uvec2, OwnerType>>,       \
    draw_property_widget<ScalarPropertyWidget<glm::uvec3, OwnerType>>,       \
    draw_property_widget<ScalarPropertyWidget<glm::uvec4, OwnerType>>,       \
    draw_property_widget<BoolPropertyWidget<OwnerType>>,                     \
    draw_property_widget<MatrixPropertyWidget<glm::mat3x4, OwnerType>>,      \
    draw_property_widget<MatrixPropertyWidget<glm::mat4x4, OwnerType>>,      \
    draw_property_widget<QuatPropertyWidget<OwnerType>>,                     \
    draw_property_widget<StringPropertyWidget<OwnerType>>,                   \
    draw_property_widget<ArrayPropertyWidget<OwnerType>, ArrayProperty>,     \
    draw_property_widget<ObjectPropertyWidget<OwnerType>>,                   \
    draw_property_widget<StructPropertyWidget<OwnerType>>,                   \
    draw_property_widget<EnumPropertyWidget<OwnerType>>


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

    if (draw_callbacks && draw_callbacks->post_draw_properties) {
        draw_callbacks->post_draw_properties(object);
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