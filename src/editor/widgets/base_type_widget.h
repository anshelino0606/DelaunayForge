#pragma once

#include "base_property_widget.h"

namespace fem {

template<typename OwnerType>
class BaseTypeWidget : public BasePropertyWidget<OwnerType> {
protected:
    DEFINE_PROPERTY_WIDGET(BaseTypeWidget, BasePropertyWidget<OwnerType>);

    bool draw_type_header() {
        std::string label = this->get_member_label();
        return ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    }
};

}