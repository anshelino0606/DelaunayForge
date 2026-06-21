#pragma once

#include "base_property_widget.h"
#include "log_categories.h"

namespace fem {

template<typename MatrixType, typename OwnerType_>
class MatrixPropertyWidget : public BasePropertyWidget<OwnerType_> {
public:
    DEFINE_PROPERTY_WIDGET(MatrixPropertyWidget, BasePropertyWidget<OwnerType_>);

    bool draw() {
        LOGT_ERROR(LogEditor, "Matrices is not supported by UI!");
        return false;
    }
};

}