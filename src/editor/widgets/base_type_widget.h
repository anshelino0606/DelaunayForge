#pragma once

#include "base_property_widget.h"

namespace fem {

template<typename OwnerType_>
class BaseTypeWidget : public BasePropertyWidget<OwnerType_> {
protected:
    DEFINE_PROPERTY_WIDGET(BaseTypeWidget, BasePropertyWidget<OwnerType_>);

    bool draw_type_header() {
        std::string label = this->get_member_label();
        return ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    }
};

}