#pragma once

#include "base_type_widget.h"
#include "widget.h"

namespace fem {

template<typename OwnerType>
class StructPropertyWidget : public BaseTypeWidget<OwnerType> {
public:
    DEFINE_PROPERTY_WIDGET(StructPropertyWidget, BaseTypeWidget<OwnerType>);

    bool draw() {
        if (!this->pre_draw())
            return false;

        bool is_changed = false;
        StructContext struct_context = this->member_->get_value_as_struct(this->owner_);

        if (struct_property_attribute<NoTypeHeader>()) {
            is_changed = WidgetInternal::draw_members(struct_context.object, struct_context.type_info);
        } else {
            if (this->draw_type_header()) {
                ImGui::Indent();
                is_changed = WidgetInternal::draw_members(struct_context.object, struct_context.type_info);
                ImGui::Unindent();
            }
        }

        this->post_draw(is_changed);
        return is_changed;
    }

    template<typename AttributeType>
    const AttributeType* struct_property_attribute() const{
        return this->template member_attribute<AttributeType>();
    }
};

}