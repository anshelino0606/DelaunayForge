#pragma once

#include "base_property_widget.h"

namespace fem {

template<typename OwnerType>
class BoolPropertyWidget : public BasePropertyWidget<OwnerType> {
public:
    DEFINE_PROPERTY_WIDGET(BoolPropertyWidget, BasePropertyWidget<OwnerType>);

    bool draw() {
        if (!this->pre_draw()) 
            return false;
        
        bool& value = this->template member_value<bool>();
        bool is_changed = ImGui::Checkbox(this->get_member_label().data(), &value);
        this->post_draw(is_changed);
        return is_changed;
    }
};

}