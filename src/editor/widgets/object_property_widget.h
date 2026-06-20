#pragma once

#include "base_type_widget.h"
#include "widget.h"

namespace fem {

template<typename OwnerType>
class ObjectPropertyWidget : public BaseTypeWidget<OwnerType> {
public:
    DEFINE_PROPERTY_WIDGET(ObjectPropertyWidget, BaseTypeWidget<OwnerType>);

    bool draw() {
        if (!this->pre_draw())
            return false;

        bool is_changed = false;
        Object* object_value = property_as_object();

        if (object_property_attribute<NoTypeHeader>()) {
            draw_base_class_combo();
            is_changed = WidgetInternal::draw_members(object_value, object_value->get_type_info());
        } else {
            if (this->draw_type_header()) {
                ImGui::Indent();
                draw_base_class_combo();
                is_changed = WidgetInternal::draw_members(object_value, object_value->get_type_info());
                ImGui::Unindent();
            }
        }

        this->post_draw(is_changed);
        return is_changed;
    }

private:
    void draw_base_class_combo() {
        if (!object_property_attribute<BaseClass>() && !object_type_attribute<BaseClass>()) {
            return;
        }

        Object* current_object_value = property_as_object();
        const ObjectTypeInfo* current_type_info = current_object_value->get_type_info();
        const ObjectTypeInfo* base_type_info = static_cast<const ObjectTypeInfo*>(this->member_->get_type_info());

        std::string label_name = std::format("{} Class", this->get_member_label());

        if (ImGui::BeginCombo(label_name.c_str(), this->get_type_label(current_type_info).data())) {
            for (const ObjectTypeInfo* type_info : base_type_info->get_children_type_infos()) {
                if (base_type_info->has_attribute<AbstractClass>() && type_info == base_type_info) {
                    continue;
                }

                bool is_selected = current_type_info->is_exactly(type_info);
                std::string type_info_label = this->get_type_label(type_info);

                if (ImGui::Selectable(type_info_label.c_str(), is_selected) && !is_selected) {
                    Object* new_object_value = create_object(type_info);
                    this->member_->set_value(this->owner_, new_object_value);
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
    }

    Object* property_as_object() {
        return this->member_->get_value_as_object(this->owner_);
    }

    template<typename AttributeType>
    const AttributeType* object_property_attribute() const {
        return this->template member_attribute<AttributeType>();
    }

    template<typename AttributeType>
    const AttributeType* object_type_attribute() const {
        return this->member_->get_type_info()->template get_attribute<AttributeType>();
    }
};

}