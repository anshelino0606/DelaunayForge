#include "widgets.h"
#include "log_categories.h"
#include "core/object/object.h"
#include "core/object/property.h"
#include "core/object/type_attribute.h"
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <functional>
#include <vector>

namespace fem {

uint64_t g_unique_id = 0;

using PostDrawCallback = std::function<void()>;

std::vector<PostDrawCallback> g_post_draw_callbacks;

void add_post_draw_callback(const PostDrawCallback& callback) {
    g_post_draw_callbacks.push_back(callback);
}

#define FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(Type, ScalarType, ImGuiType, ElementCount, Format) \
    template<>                                                                                  \
    struct ScalarPropertyUITraits<Type> {                                                       \
        using ValueType = ScalarType;                                                           \
        static constexpr ImGuiDataType data_type = ImGuiType;                                   \
        static constexpr uint32_t element_count = ElementCount;                                 \
        static constexpr std::string_view format = Format;                                      \
    };

FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(int32_t, int32_t, ImGuiDataType_S32, 1, "%d");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(uint32_t, uint32_t, ImGuiDataType_U32, 1, "%u");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(int64_t, int64_t, ImGuiDataType_S64, 1, "%lld");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(uint64_t, uint64_t, ImGuiDataType_U64, 1, "%llu");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(float, float, ImGuiDataType_Float, 1, "%.3f");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(double, double, ImGuiDataType_Double, 1, "%.3f");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::vec2, float, ImGuiDataType_Float, 2, "%.3f");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::vec3, float, ImGuiDataType_Float, 3, "%.3f");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::vec4, float, ImGuiDataType_Float, 4, "%.3f");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::dvec2, double, ImGuiDataType_Double, 2, "%.3f");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::dvec3, double, ImGuiDataType_Double, 3, "%.3f");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::dvec4, double, ImGuiDataType_Double, 4, "%.3f");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::ivec2, int32_t, ImGuiDataType_S32, 2, "%d");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::ivec3, int32_t, ImGuiDataType_S32, 3, "%d");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::ivec4, int32_t, ImGuiDataType_S32, 4, "%d");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::uvec2, uint32_t, ImGuiDataType_U32, 2, "%u");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::uvec3, uint32_t, ImGuiDataType_U32, 3, "%u");
FEM_DEFINE_SCALAR_PROPERTY_UI_TRAITS(glm::uvec4, uint32_t, ImGuiDataType_U32, 4, "%u");

std::string_view get_label(std::string_view name) {
    static std::string result_string;
    result_string.clear();

    bool is_prev_char_space = false;
    bool is_prev_char_upper = false;
    size_t name_len = name.length();

    for (size_t i = 0; i != name_len; ++i) {
        char c = name[i];

        if (!result_string.empty() && std::isupper(c) && !is_prev_char_space && !is_prev_char_upper) {
            result_string += ' ';
        }

        is_prev_char_space = c == ' ';
        is_prev_char_upper = std::isupper(c);

        result_string += c;
    }

    return result_string;
}

std::string_view get_property_label(Property* property) {
    const DisplayName* display_name_attr = property->get_attribute<DisplayName>();
    return get_label(display_name_attr ? display_name_attr->display_name : property->display_name());
}

std::string_view get_function_label(Function* function) {
    const DisplayName* display_name_attr = function->get_attribute<DisplayName>();
    return get_label(display_name_attr ? display_name_attr->display_name : function->display_name());
}

std::string_view get_type_label(const TypeInfo* type_info) {
    const DisplayName* display_name_attr = type_info->get_attribute<DisplayName>();
    return get_label(display_name_attr ? display_name_attr->display_name : type_info->get_name());
}

struct DrawObjectInfo {
    bool need_header = true;
};

void draw_root_object(Object* owner_object, DrawObjectInfo info = DrawObjectInfo());
void draw_root_object(Struct* owner_object, const StructTypeInfo* type_info, DrawObjectInfo info = DrawObjectInfo());

template<typename ScalarType, typename T>
bool draw_scalar_property(T* owner_object, Property* property) {
    using Traits = ScalarPropertyUITraits<ScalarType>;
    using ValueType = typename Traits::ValueType;

    const ClampMin* clamp_min_attr = property->get_attribute<ClampMin>();
    const ClampMax* clamp_max_attr = property->get_attribute<ClampMax>();
    const DragSpeed* drag_speed_attr = property->get_attribute<DragSpeed>();
    const Format* format_attr = property->get_attribute<Format>();

    const bool is_slider = clamp_min_attr && clamp_max_attr && !drag_speed_attr;

    float drag_speed = drag_speed_attr ? drag_speed_attr->speed : 1.0f;
    std::string_view label_name = get_property_label(property);
    std::string_view format = format_attr ? format_attr->format : "";
    ImGuiSliderFlags flags = 0;

    ValueType min = clamp_min_attr ? static_cast<ValueType>(clamp_min_attr->min) : 0;
    ValueType max = clamp_max_attr ? static_cast<ValueType>(clamp_max_attr->max) : 0;
    ScalarType& data = property->get_value<ScalarType>(owner_object);
    
    if (is_slider) {
        return Widgets::slider_scalar(label_name, data, min, max, flags, format);
    } else {
        return Widgets::drag_scalar(label_name, data, drag_speed, min, max, flags, format);
    }
}

template<typename T>
void draw_array_property(T* owner_object, Property* property) {
    ArrayProperty* array_property = static_cast<ArrayProperty*>(property);
    PropertyType value_type = array_property->get_value_type();

    if (value_type != PropertyType::OBJECT && value_type != PropertyType::STRUCT) {
        LOGT_ERROR(LogEditor, "Array UI is supported only for OBJECT and STRUCT value types!");
        return;
    }

    std::string_view array_name = get_property_label(array_property);
    const TypeInfo* element_info = array_property->get_value_type_info();

    const OnArrayValueRemoved* on_removed_attr = array_property->get_attribute<OnArrayValueRemoved>();
    
    if (ImGui::CollapsingHeader(array_name.data(), ImGuiTreeNodeFlags_AllowOverlap)) {
        ImGui::SameLine();

        std::string button_name = std::format("Add Element##{}", ++g_unique_id);

        if (ImGui::Button(button_name.c_str())) {
            if (value_type == PropertyType::OBJECT) {
                Object* object_value = create_object(static_cast<const ObjectTypeInfo*>(element_info));
                array_property->add_value(owner_object, object_value);
            } else {
                array_property->emplace_value(owner_object);
            }

            if (const OnArrayValueAdded* on_added = array_property->get_attribute<OnArrayValueAdded>()) {
                on_added->on_added(owner_object);
            }
        }

        ImGui::Indent();

        size_t idx_to_erase = array_property->get_element_count(owner_object);

        for (size_t i = 0; i != array_property->get_element_count(owner_object); ++i) {
            ImGui::PushID(i);

            std::string element_name(get_type_label(element_info));
            element_name += " #" + std::to_string(i);

            bool collapsing_header_result = ImGui::CollapsingHeader(element_name.c_str(), ImGuiTreeNodeFlags_AllowOverlap);
            
            ImGui::SameLine();

            std::string remove_button_name = std::format("Remove##{}", ++g_unique_id);

            if (ImGui::Button(remove_button_name.c_str())) {
                idx_to_erase = i;

                if (on_removed_attr && on_removed_attr->on_pre_removed) {
                    on_removed_attr->on_pre_removed(owner_object, array_property->get_value_ptr(owner_object, i));
                }
            }

            if (collapsing_header_result) {
                if (value_type == PropertyType::OBJECT) {
                    Object* object_value = array_property->get_value_as_object(owner_object, i);
                    draw_root_object(object_value, { false });
                } else if (value_type == PropertyType::STRUCT) {
                    StructContext struct_context = array_property->get_value_as_struct(owner_object, i);
                    draw_root_object(struct_context.object, struct_context.type_info, { false });
                }
            }

            ImGui::PopID();
        }

        ImGui::Unindent();

        if (idx_to_erase < array_property->get_element_count(owner_object)) {
            add_post_draw_callback([idx_to_erase, array_property, owner_object, on_removed_attr] {
                array_property->erase(owner_object, idx_to_erase);

                if (on_removed_attr && on_removed_attr->on_post_removed) {
                    on_removed_attr->on_post_removed(owner_object);
                }
            });
        }
    }
}

template<typename T>
void draw_base_class_combo(T* owner_object, Property* property) {
    if (!property->has_attribute<BaseClass>() && !property->get_type_info()->has_attribute<BaseClass>()) {
        return;
    }

    Object* current_object_value = property->get_value_as_object(owner_object);
    const ObjectTypeInfo* current_type_info = current_object_value->get_type_info();
    const ObjectTypeInfo* base_type_info = static_cast<const ObjectTypeInfo*>(property->get_type_info());

    std::string label_name(get_property_label(property));
    label_name += " Class";

    if (ImGui::BeginCombo(label_name.c_str(), get_type_label(current_type_info).data())) {
        for (const ObjectTypeInfo* type_info : base_type_info->get_children_type_infos()) {
            if (base_type_info->has_attribute<AbstractClass>() && type_info == base_type_info) {
                continue;
            }

            bool is_selected = current_type_info->is_exactly(type_info);
            std::string_view type_info_name = get_type_label(type_info);

            if (ImGui::Selectable(type_info_name.data(), is_selected) && !is_selected) {
                add_post_draw_callback([property, type_info, owner_object](){
                    Object* new_object_value = create_object(type_info);
                    property->set_value(owner_object, new_object_value);
                });
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
}

template<typename T>
bool draw_enum_property(T* owner_object, Property* property) {
    bool is_changed = false;
    
    EnumContext enum_context = property->get_value_as_enum(owner_object);

    Enum* object = enum_context.object;
    const EnumTypeInfo* enum_info = enum_context.type_info;

    std::string_view label_name = get_property_label(property);
    std::string_view current_enum_name = enum_info->get_value(object);

    if (const DrawAsToggles* draw_as_toggles_attr = enum_info->get_attribute<DrawAsToggles>()) {
        ImGui::Text(label_name.data());

        if (draw_as_toggles_attr->same_line_with_label) {
            ImGui::SameLine();
        }

        enum_info->for_each_element([&](std::string_view enum_name) {
            bool is_selected = current_enum_name == enum_name;

            if (ImGui::RadioButton(enum_name.data(), is_selected)) {
                enum_info->set_value(object, enum_name);
            }

            ImGui::SameLine();
        });

        ImGui::NewLine();
    } else {
        if (ImGui::BeginCombo(label_name.data(), current_enum_name.data())) {
            enum_info->for_each_element([&](std::string_view enum_name) {
                bool is_selected = current_enum_name == enum_name;

                if (ImGui::Selectable(enum_name.data(), is_selected)) {
                    enum_info->set_value(object, enum_name);
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            });

            ImGui::EndCombo();
        }
    }

    std::string_view new_enum_name = enum_info->get_value(object);
    if (current_enum_name != new_enum_name) {
        is_changed = true;
    }

    return is_changed;
}

template<typename T>
bool draw_members(T* owner_object, const StructTypeInfo* type_info) {
    const std::vector<Property*>& properties = type_info->get_properties();
    const DrawCallbacks* draw_callbacks = type_info->get_attribute<DrawCallbacks>();

    if (draw_callbacks && draw_callbacks->pre_draw_properties) {
        draw_callbacks->pre_draw_properties(owner_object);
    }

    bool is_owner_object_changed = false;

    for (Member* member : type_info->get_members()) {
        if (member->has_attribute<NoUI>()) {
            continue;
        }

        if (const EditConditionMember* edit_condition = member->get_attribute<EditConditionMember>()) {
            if (!edit_condition->evaluate(owner_object)) {
                continue;
            }
        }

        if (const EditConditionFlags* edit_flags = member->get_attribute<EditConditionFlags>()) {
            if (!edit_flags->evaluate(owner_object, type_info)) {
                continue;
            }
        }

        ImGui::PushID(member);

        if (member->has_attribute<SameLine>()) {
            ImGui::SameLine();
        }

        switch (member->member_type()) {
        case MemberType::PROPERTY: {
            Property* property = static_cast<Property*>(member);

            const OnValueChanged* on_changed_attr = property->get_attribute<OnValueChanged>();
            bool is_changed = false;

            switch (property->get_type()) {
            case PropertyType::INT32:
                is_changed = draw_scalar_property<int32_t>(owner_object, property);
                break;
            case PropertyType::IVEC2:
                is_changed = draw_scalar_property<glm::ivec2>(owner_object, property);
                break;
            case PropertyType::IVEC3:
                is_changed = draw_scalar_property<glm::ivec3>(owner_object, property);
                break;
            case PropertyType::IVEC4:
                is_changed = draw_scalar_property<glm::ivec4>(owner_object, property);
                break;
            case PropertyType::UINT32:
                is_changed = draw_scalar_property<uint32_t>(owner_object, property);
                break;
            case PropertyType::UVEC2:
                is_changed = draw_scalar_property<glm::uvec2>(owner_object, property);
                break;
            case PropertyType::UVEC3:
                is_changed = draw_scalar_property<glm::uvec3>(owner_object, property);
                break;
            case PropertyType::UVEC4:
                is_changed = draw_scalar_property<glm::uvec4>(owner_object, property);
                break;
            case PropertyType::FLOAT:
                is_changed = draw_scalar_property<float>(owner_object, property);
                break;
            case PropertyType::VEC2:
                is_changed = draw_scalar_property<glm::vec2>(owner_object, property);
                break;
            case PropertyType::VEC3:
                is_changed = draw_scalar_property<glm::vec3>(owner_object, property);
                break;
            case PropertyType::VEC4:
                is_changed = draw_scalar_property<glm::vec4>(owner_object, property);
                break;
            case PropertyType::DOUBLE:
                is_changed = draw_scalar_property<double>(owner_object, property);
                break;
            case PropertyType::DVEC2:
                is_changed = draw_scalar_property<glm::dvec2>(owner_object, property);
                break;
            case PropertyType::DVEC3:
                is_changed = draw_scalar_property<glm::dvec3>(owner_object, property);
                break;
            case PropertyType::DVEC4:
                is_changed = draw_scalar_property<glm::dvec4>(owner_object, property);
                break;
            case PropertyType::BOOL: {
                bool& value = property->get_value<bool>(owner_object);
                is_changed = ImGui::Checkbox(get_property_label(property).data(), &value);
                break;
            }
            case PropertyType::STRING: {
                std::string& value = property->get_value<std::string>(owner_object);
                is_changed = ImGui::InputText(get_property_label(property).data(), &value);
                break;
            }
            case PropertyType::STRUCT: {
                StructContext struct_context = property->get_value_as_struct(owner_object);

                if (property->has_attribute<NoTypeHeader>()) {
                    is_changed = draw_members(struct_context.object, struct_context.type_info);
                } else {
                    std::string_view property_label = get_property_label(property);
                    if (ImGui::CollapsingHeader(property_label.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Indent();
                        is_changed = draw_members(struct_context.object, struct_context.type_info);
                        ImGui::Unindent();
                    }
                }
                break;
            }
            case PropertyType::OBJECT: {
                Object* object_value = property->get_value_as_object(owner_object);

                if (property->has_attribute<NoTypeHeader>()) {
                    draw_base_class_combo(owner_object, property);
                    is_changed = draw_members(object_value, object_value->get_type_info());
                } else {
                    std::string_view property_label = get_property_label(property);
                    if (ImGui::CollapsingHeader(property_label.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Indent();
                        draw_base_class_combo(owner_object, property);
                        is_changed = draw_members(object_value, object_value->get_type_info());
                        ImGui::Unindent();
                    }
                }

                break;
            }
            case PropertyType::ENUM: {
                is_changed = draw_enum_property(owner_object, property);
                break;
            };
            case PropertyType::ARRAY: {
                draw_array_property(owner_object, property);
                break;
            }
            case PropertyType::MAT3X4: {
                LOGT_ERROR(LogEditor, "MAT3X4 is not supported by UI!");
                break;
            }
            case PropertyType::MAT4X4: {
                LOGT_ERROR(LogEditor, "MAT4X4 is not supported by UI!");
                break;
            }
            case PropertyType::QUAT: {
                LOGT_ERROR(LogEditor, "QUAT is not supported by UI!");
                break;
            }
            default:
                break;
            }

            if (is_changed && on_changed_attr) {
                on_changed_attr->on_value_changed(owner_object);
            }

            is_owner_object_changed |= is_changed;

            break;
        }
        case MemberType::FUNCTION: {
            Function* function = static_cast<Function*>(member);

            std::string_view label = get_function_label(function);
            std::string button_name = std::format("{}##{}", label, ++g_unique_id);

            if (ImGui::Button(button_name.c_str())) {
                function->invoke(owner_object, {});
            }

            break;
        }
        }

        ImGui::PopID();
    }

    if (draw_callbacks && draw_callbacks->post_draw_properties) {
        draw_callbacks->post_draw_properties(owner_object);
    }

    return is_owner_object_changed;
}

void draw_root_object(Object* owner_object, DrawObjectInfo info) {
    if (owner_object->has_attribute<NoTypeHeader>() || !info.need_header) {
        draw_members(owner_object, owner_object->get_type_info());
    } else {
        std::string_view type_label_name = get_type_label(owner_object->get_type_info());
        if (ImGui::CollapsingHeader(type_label_name.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_members(owner_object, owner_object->get_type_info());
        }
    }
}

void draw_root_object(Struct* owner_object, const StructTypeInfo* type_info, DrawObjectInfo info) {
    if (type_info->has_attribute<NoTypeHeader>() || !info.need_header) {
        draw_members(owner_object, type_info);
    } else {
        std::string_view type_label_name = get_type_label(type_info);
        if (ImGui::CollapsingHeader(type_label_name.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_members(owner_object, type_info);
        }
    }
}


void Widgets::draw_object(Object* object) {
    draw_root_object(object);
}

void Widgets::draw_struct(Struct* object, const StructTypeInfo* type_info) {
    draw_root_object(object, type_info);
}

void Widgets::execute_post_draw_callbacks() {
    for (const PostDrawCallback& callback : g_post_draw_callbacks) {
        callback();
    }
    g_post_draw_callbacks.clear();

    // Not ideal place to reset this one. But ok for now
    // TODO: move this to a more appropriate place
    g_unique_id = 0;
}

}