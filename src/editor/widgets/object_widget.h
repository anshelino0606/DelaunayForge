#pragma once

#include "base_widget.h"
#include "widget_internal.h"
#include "core/object/object.h"
#include <imgui.h>

namespace fem {

class ObjectWidget : public BaseWidget {
public:
    void draw(Object* object) {
        const ObjectTypeInfo* type_info = object->get_type_info();

        if (type_info->has_attribute<NoTypeHeader>()) {
            WidgetInternal::draw_members(object, type_info);
        } else {
            std::string type_label_name = get_type_label(type_info);
            if (ImGui::CollapsingHeader(type_label_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                WidgetInternal::draw_members(object, type_info);
            }
        }
    }
};

}