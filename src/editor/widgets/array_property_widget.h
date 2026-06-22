#pragma once

#include "base_member_widget.h"
#include "widget_internal.h"
#include "log_categories.h"

namespace fem {

template<typename OwnerType_>
class ArrayPropertyWidget : public BaseMemberWidget<ArrayProperty, OwnerType_> {
public:
    using OwnerType = OwnerType_;
    using Base = BaseMemberWidget<ArrayProperty, OwnerType>;
    
    ArrayPropertyWidget(ArrayProperty* property, OwnerType* owner, const StructTypeInfo* owner_type_info)
        : Base(property, owner, owner_type_info) {}

    bool draw() {
        if (!this->pre_draw())
            return false;

        if (!init_internal())
            return false;

        std::string array_name = this->get_member_label();
        
        if (!draw_header(array_name))
        {
            this->post_draw(false);
            return false;
        }
            
        ImGui::SameLine();
        draw_add_button();
        ImGui::Indent();

        std::string element_label = this->get_type_label(element_type_info_);

        for (size_t i = 0; i < this->member_->get_element_count(this->owner_); ++i) {
            draw_element_header(element_label, i);
        }

        ImGui::Unindent();

        this->post_draw(false);
        return false;
    }

private:
    PropertyType element_type_ = PropertyType::COUNT;
    const TypeInfo* element_type_info_ = nullptr;

    bool init_internal() {
        element_type_ = this->member_->get_value_type();

        if (element_type_ != PropertyType::OBJECT && element_type_ != PropertyType::STRUCT) {
            LOGT_ERROR(LogEditor, "Array UI is supported only for OBJECT and STRUCT value types!");
            return false;
        }

        element_type_info_ = this->member_->get_value_type_info();

        return true;
    }

    void draw_add_button() {
        if (ImGui::Button("Add Element")) {
            if (element_type_ == PropertyType::OBJECT) {
                Object* object_value = create_object(static_cast<const ObjectTypeInfo*>(element_type_info_));
                this->member_->add_value(this->owner_, object_value);
            } else {
                this->member_->emplace_value(this->owner_);
            }
        }
    }

    void draw_element_header(const std::string& element_label, size_t element_idx) {
        ImGui::PushID(element_idx);

        std::string element_label_with_idx = std::format("{} #{}", element_label, element_idx);
        bool element_header_result = draw_header(element_label_with_idx);

        ImGui::SameLine();

        if (ImGui::Button("Remove")) {
            this->member_->erase(this->owner_, element_idx);
        }

        if (element_header_result) {
            if (element_type_ == PropertyType::OBJECT) {
                Object* object_value = this->member_->get_value_as_object(this->owner_, element_idx);
                WidgetInternal::draw_members(object_value, struct_type_info());
            } else if (element_type_ == PropertyType::STRUCT) {
                StructContext struct_context = this->member_->get_value_as_struct(this->owner_, element_idx);
                WidgetInternal::draw_members(struct_context.object, struct_type_info());
            }
        }

        ImGui::PopID();
    }

    bool draw_header(const std::string& label) {
        return ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_AllowOverlap);
    }

    const StructTypeInfo* struct_type_info() const {
        return static_cast<const StructTypeInfo*>(element_type_info_);
    }
};

}