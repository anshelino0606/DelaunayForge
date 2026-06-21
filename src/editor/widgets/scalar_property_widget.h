#pragma once

#include "base_property_widget.h"
#include "scalar_widgets.h"

namespace fem {

template<typename ScalarType, typename OwnerType_>
class ScalarPropertyWidget : public BasePropertyWidget<OwnerType_> {
public:
    DEFINE_PROPERTY_WIDGET(ScalarPropertyWidget, BasePropertyWidget<OwnerType_>);

    bool draw() {
        if (!init_internal())
            return false;

        bool is_changed = false;

        if (is_slider()) {
            is_changed = draw_slider();
        } else {
            is_changed = draw_drag();
        }

        this->post_draw(is_changed);

        return is_changed;
    }

private:
    using Traits = ScalarPropertyUITraits<ScalarType>;
    static constexpr ImGuiSliderFlags s_flags = 0;

    const ClampMin* clamp_min_attr_ = nullptr;
    const ClampMax* clamp_max_attr_ = nullptr;
    const DragSpeed* drag_speed_attr_ = nullptr;
    const Format* format_attr_ = nullptr;

    bool init_internal() {
        if (!this->pre_draw())
            return false;

        clamp_min_attr_ = this->template member_attribute<ClampMin>();
        clamp_max_attr_ = this->template member_attribute<ClampMax>();
        drag_speed_attr_ = this->template member_attribute<DragSpeed>();
        format_attr_ = this->template member_attribute<Format>();

        return true;
    }

    bool draw_slider() {
        static SliderWidget<ScalarType> widget;
        return widget
            .set_value(scalar_value())
            .set_range(min_value(), max_value())
            .set_flags(s_flags)
            .set_format(user_format())
            .draw(this->get_member_label());
    }

    bool draw_drag() {
        static DragWidget<ScalarType> widget;
        return widget
            .set_value(scalar_value())
            .set_range(min_value(), max_value())
            .set_drag_speed(drag_speed())
            .set_flags(s_flags)
            .set_format(user_format())
            .draw(this->get_member_label());
    }

    bool is_slider() const {
        return clamp_min_attr_ && clamp_max_attr_ && !drag_speed_attr_;
    }

    float drag_speed() const {
        return drag_speed_attr_ ? drag_speed_attr_->speed : 1.0f;
    }

    Traits::ValueType min_value() const {
        return clamp_min_attr_ ? static_cast<Traits::ValueType>(clamp_min_attr_->min) : 0;
    }

    Traits::ValueType max_value() const {
        return clamp_max_attr_ ? static_cast<Traits::ValueType>(clamp_max_attr_->max) : 0;
    }

    std::string_view user_format() const {
        return format_attr_ ? format_attr_->format : "";
    }

    ScalarType& scalar_value() {
        return this->template member_value<ScalarType>();
    }
};

}