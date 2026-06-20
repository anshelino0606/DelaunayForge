#pragma once

#include "base_property_widget.h"
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace fem {

template<typename OwnerType>
class StringPropertyWidget : public BasePropertyWidget<OwnerType> {
public:
    DEFINE_PROPERTY_WIDGET(StringPropertyWidget, BasePropertyWidget<OwnerType>);

    bool draw() {
        if (!this->pre_draw())
            return false;
        
        std::string& value = this->template member_value<std::string>();
        bool is_changed = ImGui::InputText(this->get_member_label().data(), &value);
        this->post_draw(is_changed);
        return is_changed;
    }
};

}