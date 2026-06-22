#pragma once

#include <imgui.h>
#include <glm/glm.hpp>
#include <string_view>

namespace fem {

template<typename T>
struct ScalarDefaults;

template<typename T, typename = void>
struct ScalarPropertyUITraits;

template<> struct ScalarDefaults<int32_t> { 
    static constexpr ImGuiDataType type = ImGuiDataType_S32;    
    static constexpr std::string_view fmt = "%d"; 
};

template<> struct ScalarDefaults<uint32_t> { 
    static constexpr ImGuiDataType type = ImGuiDataType_U32;    
    static constexpr std::string_view fmt = "%u"; 
};

template<> struct ScalarDefaults<int64_t> { 
    static constexpr ImGuiDataType type = ImGuiDataType_S64;    
    static constexpr std::string_view fmt = "%lld"; 
};

template<> struct ScalarDefaults<uint64_t> { 
    static constexpr ImGuiDataType type = ImGuiDataType_U64;    
    static constexpr std::string_view fmt = "%llu"; 
};

template<> struct ScalarDefaults<float> { 
    static constexpr ImGuiDataType type = ImGuiDataType_Float;  
    static constexpr std::string_view fmt = "%.3f"; 
};

template<> struct ScalarDefaults<double> { 
    static constexpr ImGuiDataType type = ImGuiDataType_Double; 
    static constexpr std::string_view fmt = "%.3f"; 
};

template<typename T>
requires requires { ScalarDefaults<T>::type; }
struct ScalarPropertyUITraits<T> {
    using ValueType = T;
    static constexpr ImGuiDataType data_type = ScalarDefaults<T>::type;
    static constexpr uint32_t element_count = 1;
    static constexpr std::string_view format = ScalarDefaults<T>::fmt;
};

template<typename T>
requires requires { typename T::value_type; { T::length() } -> std::same_as<int>; }
struct ScalarPropertyUITraits<T> {
    using ValueType = typename T::value_type;
    static constexpr ImGuiDataType data_type = ScalarDefaults<ValueType>::type;
    static constexpr uint32_t element_count = static_cast<uint32_t>(T::length());
    static constexpr std::string_view format = ScalarDefaults<ValueType>::fmt;
};

template<typename ScalarType, typename Derived>
class BaseScalarWidget {
public:
    using Traits = ScalarPropertyUITraits<ScalarType>;

    Derived& set_value(ScalarType& value) {
        value_ = &value;
        return get_derived();
    }

    Derived& set_range(Traits::ValueType min_value, Traits::ValueType max_value) {
        min_value_ = min_value;
        max_value_ = max_value;
        return get_derived();
    }

    Derived& set_flags(ImGuiSliderFlags flags) {
        flags_ = flags;
        return get_derived();
    }

    Derived& set_format(std::string_view format) {
        format_ = format;
        return get_derived();
    }

protected:
    ScalarType* value_ = nullptr;
    Traits::ValueType min_value_ = 0;
    Traits::ValueType max_value_ = 0;
    ImGuiSliderFlags flags_;
    std::string_view format_ = "";

    std::string_view get_user_format() const {
        return this->format_.empty() ? Traits::format : this->format_;
    }

private:
    Derived& get_derived() {
        return *static_cast<Derived*>(this);
    }
};

template<typename ScalarType>
class DragWidget : public BaseScalarWidget<ScalarType, DragWidget<ScalarType>> {
public:
    using Base = BaseScalarWidget<ScalarType, DragWidget<ScalarType>>;

    DragWidget<ScalarType>& set_drag_speed(float drag_speed) {
        drag_speed_ = drag_speed;
        return *this;
    }

    bool draw(std::string_view label) const {
        return ImGui::DragScalarN(
            label.data(), 
            Base::Traits::data_type, 
            this->value_, 
            Base::Traits::element_count, 
            drag_speed_, 
            &this->min_value_, 
            &this->max_value_, 
            this->get_user_format().data(), 
            this->flags_
        );
    }

protected:
    float drag_speed_ = 1.0f;
};

template<typename ScalarType>
class SliderWidget : public BaseScalarWidget<ScalarType, SliderWidget<ScalarType>> {
public:
    using Base = BaseScalarWidget<ScalarType, SliderWidget<ScalarType>>;

    bool draw(std::string_view label) const {
        return ImGui::SliderScalarN(
            label.data(), 
            Base::Traits::data_type, 
            this->value_, 
            Base::Traits::element_count, 
            &this->min_value_, 
            &this->max_value_, 
            this->get_user_format().data(), 
            this->flags_
        );
    }
};

}