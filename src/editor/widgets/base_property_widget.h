#pragma once

#include "base_member_widget.h"

namespace fem {

template<typename OwnerType>
class BasePropertyWidget : public BaseMemberWidget<Property, OwnerType> {
protected:
    using Base = BaseMemberWidget<Property, OwnerType>;

    BasePropertyWidget(Property* property, OwnerType* owner, const StructTypeInfo* owner_type_info)
        : Base(property, owner, owner_type_info) {}

    template<typename ValueType>
    ValueType& member_value() {
        return this->member_->template get_value<ValueType>(this->owner_);
    }
};

#define DEFINE_PROPERTY_WIDGET(WidgetType, WidgetBaseType) \
    WidgetType(Property* property, OwnerType* owner, const StructTypeInfo* owner_type_info) : \
        WidgetBaseType(property, owner, owner_type_info) {}

}