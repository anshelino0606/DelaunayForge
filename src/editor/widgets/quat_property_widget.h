#pragma once

#include "base_property_widget.h"
#include "log_categories.h"

namespace fem {

template<typename OwnerType_>
class QuatPropertyWidget : public BasePropertyWidget<OwnerType_> {
public:
    DEFINE_PROPERTY_WIDGET(QuatPropertyWidget, BasePropertyWidget<OwnerType_>);

    bool draw() {
        LOGT_ERROR(LogEditor, "Quaternions is not supported by UI!");
        return false;
    }
};

}