#pragma once

#include "base_type_widget.h"

namespace fem {

template<typename OwnerType_>
class EnumPropertyWidget : public BaseTypeWidget<OwnerType_> {
public:
    DEFINE_PROPERTY_WIDGET(EnumPropertyWidget, BaseTypeWidget<OwnerType_>);

    bool draw() {
        if (!init_internal())
            return false;

        bool is_changed = false;

        std::string label_name = this->get_member_label();

        if (draw_as_toggles_attr_) {
            draw_toggle();
        } else {
            draw_combo_box();
        }

        std::string_view new_enum_name = enum_info_->get_value(enum_object_);
        if (current_enum_value_ != new_enum_name) {
            is_changed = true;
        }

        this->post_draw(is_changed);

        return is_changed;
    }

private:
    Enum* enum_object_ = nullptr;
    const EnumTypeInfo* enum_info_ = nullptr;
    std::string_view current_enum_value_ = "";
    const DrawAsToggles* draw_as_toggles_attr_ = nullptr;

    bool init_internal() {
        if (!this->pre_draw())
            return false;

        EnumContext enum_context = this->member_->get_value_as_enum(this->owner_);

        enum_object_ = enum_context.object;
        enum_info_ = enum_context.type_info;

        current_enum_value_ = enum_info_->get_value(enum_object_);
        draw_as_toggles_attr_ = enum_info_->get_attribute<DrawAsToggles>();

        return true;
    }

    void draw_toggle() {
        std::string label_name = this->get_member_label();

        ImGui::Text(label_name.c_str());

        if (draw_as_toggles_attr_->same_line_with_label) {
            ImGui::SameLine();
        }

        enum_info_->for_each_element([&](std::string_view enum_name) {
            bool is_selected = current_enum_value_ == enum_name;

            if (ImGui::RadioButton(enum_name.data(), is_selected)) {
                enum_info_->set_value(enum_object_, enum_name);
            }

            ImGui::SameLine();
        });

        ImGui::NewLine();
    }

    void draw_combo_box() {
        std::string label_name = this->get_member_label();

        if (ImGui::BeginCombo(label_name.c_str(), current_enum_value_.data())) {
            enum_info_->for_each_element([&](std::string_view enum_name) {
                bool is_selected = current_enum_value_ == enum_name;

                if (ImGui::Selectable(enum_name.data(), is_selected)) {
                    enum_info_->set_value(enum_object_, enum_name);
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            });

            ImGui::EndCombo();
        }
    }
};

}