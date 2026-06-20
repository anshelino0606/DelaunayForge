#pragma once

#include "base_widget.h"
#include "core/object/property.h"
#include "core/object/function.h"
#include <imgui.h>

namespace fem {

template <typename T>
concept ReflectableType = std::is_base_of_v<Property, T> ||
                          std::is_base_of_v<Function, T> ||
                          std::is_base_of_v<TypeInfo, T>;

template<typename MemberType, typename OwnerType>
class BaseMemberWidget : public BaseWidget {
public:
    BaseMemberWidget(MemberType* member, OwnerType* owner, const StructTypeInfo* owner_type_info)
        : member_(member), owner_(owner), owner_type_info_(owner_type_info) 
    {
        assert(member_ && owner_ && owner_type_info_);
    }

protected:
    MemberType* member_ = nullptr;
    OwnerType* owner_ = nullptr;
    const StructTypeInfo* owner_type_info_ = nullptr;

    bool pre_draw() {
        assert(member_ && owner_);
        if (member_->template has_attribute<NoUI>()) {
            return false;
        }

        if (const EditConditionMember* edit_condition = member_->template get_attribute<EditConditionMember>()) {
            if (!edit_condition->evaluate(owner_)) {
                return false;
            }
        }

        if (const EditConditionFlags* edit_flags = member_->template get_attribute<EditConditionFlags>()) {
            if (!edit_flags->evaluate(owner_, owner_type_info_)) {
                return false;
            }
        }

        ImGui::PushID(member_);

        if (member_->template has_attribute<SameLine>()) {
            ImGui::SameLine();
        }

        return true;
    }

    void post_draw(bool is_widget_changed) {
        if constexpr (std::is_base_of_v<Property, MemberType>) {
            const OnValueChanged* on_changed_attr = member_->template get_attribute<OnValueChanged>();
            if (is_widget_changed && on_changed_attr) {
                on_changed_attr->on_value_changed(owner_);
            }
        }

        ImGui::PopID();
    }

    std::string get_member_label() {
        const DisplayName* display_name_attr = member_->template get_attribute<DisplayName>();
        return get_label(display_name_attr ? display_name_attr->display_name : member_->display_name());
    }

    std::string get_owner_label() {
        const DisplayName* display_name_attr = owner_->template get_attribute<DisplayName>();
        return get_label(display_name_attr ? display_name_attr->display_name : owner_->display_name());
    }

    template<typename AttributeType>
    const AttributeType* member_attribute() const {
        return member_->template get_attribute<AttributeType>();
    }

    template<typename AttributeType>
    const AttributeType* owner_attribute() const {
        return owner_->template get_attribute<AttributeType>();
    }
};

}