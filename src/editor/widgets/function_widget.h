#pragma once

#include "base_member_widget.h"

namespace fem {

template<typename OwnerType>
class FunctionWidget : public BaseMemberWidget<Function, OwnerType> {
public:
    using Base = BaseMemberWidget<Function, OwnerType>;
    
    FunctionWidget(Function* function, OwnerType* owner, const StructTypeInfo* owner_type_info)
        : Base(function, owner, owner_type_info) {}

    bool draw() {
        if (!this->pre_draw())
            return false;

        std::string label = this->get_member_label();

        if (ImGui::Button(label.c_str())) {
            this->member_->invoke(this->owner_, {});
        }

        this->post_draw(false);

        return false;
    }
};

}