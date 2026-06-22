#ifndef FEM_PDE_PARAMETER_H
#define FEM_PDE_PARAMETER_H

#include "core/object/object.h"
#include "core/object/property.h"
#include "math/differential_equation.h"
#include "math/math_.h"

#include <cmath>

namespace fem {

template<typename T>
class TPDEParameterValue {
protected:
    T value_;
};

class PDEDynamicParameterConfig {
protected:
    bool   time_varying_  = false;
    double amplitude_     = 0.0;
    double frequency_hz_  = 1.0;
    double phase_rad_     = 0.0;
    double offset_        = 0.0;

    double calc_dynamic_value(double t, double value) const {
        return value + offset_ + amplitude_ * std::sin(Math::two_pi * frequency_hz_ * t + phase_rad_);
    }
};

class PDEParameter : public Object {
public:
    FEM_DECLARE_OBJECT(PDEParameter);
    FEM_DECLARE_PROPERTY_REGISTER(PDEParameter);

    virtual void apply(DifferentialEquation& equation) const { };
};

class PDEScalarParameter : public PDEParameter, public TPDEParameterValue<double> {
public:
    FEM_DECLARE_OBJECT(PDEScalarParameter);
    FEM_DECLARE_PROPERTY_REGISTER(PDEScalarParameter);

    double value() const { return value_; }
    virtual double value([[maybe_unused]] double t) const { return value_; }
};

#define FEM_DECLARE_PDE_SCALAR_PARAMETER(TypeName, PDEMember)               \
namespace PDEParameters {                                                   \
    class TypeName : public PDEScalarParameter {                            \
    public:                                                                 \
        FEM_DECLARE_OBJECT(TypeName);                                       \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName);                            \
        virtual void apply(DifferentialEquation& equation) const override;  \
    private:                                                                \
        static constexpr auto s_member_ = &PDEMember;                       \
    };                                                                      \
    class TypeName##Dynamic                                                 \
        : public TypeName, public PDEDynamicParameterConfig {               \
    public:                                                                 \
        FEM_DECLARE_OBJECT(TypeName##Dynamic);                              \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName##Dynamic);                   \
        virtual double value([[maybe_unused]] double t) const override;     \
    private:                                                                \
        static constexpr auto s_member_ = &PDEMember;                       \
    };}

#define FEM_DECLARE_PDE_SCALAR_PARAMETER_DEFAULT_VALUE(TypeName, PDEMember, Value)      \
namespace PDEParameters {                                                               \
    class TypeName : public PDEScalarParameter {                                        \
    public:                                                                             \
        FEM_DECLARE_OBJECT(TypeName);                                                   \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName);                                        \
        TypeName() {                                                                    \
            value_ = Value;                                                             \
        }                                                                               \
        virtual void apply(DifferentialEquation& equation) const override;              \
    private:                                                                            \
        static constexpr auto s_member_ = &PDEMember;                                   \
    };                                                                                  \
    class TypeName##Dynamic : public TypeName, public PDEDynamicParameterConfig {       \
    public:                                                                             \
        FEM_DECLARE_OBJECT(TypeName##Dynamic);                                          \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName##Dynamic);                               \
        TypeName##Dynamic() {                                                           \
            value_ = Value;                                                             \
        }                                                                               \
        virtual double value([[maybe_unused]] double t) const override;                 \
    private:                                                                            \
        static constexpr auto s_member_ = &PDEMember;                                   \
    };}

#define FEM_REGISTER_DYNAMIC_PARAMETER_CONFIG_PROPERTIES(TypeName)                      \
    FEM_REGISTER_PROPERTY(TypeName, time_varying_, DisplayName("Time Varying")) \
    FEM_REGISTER_PROPERTY(TypeName, amplitude_, DisplayName("Amplitude"), SHOW_WHEN_MEMBER(TypeName, time_varying_, val), DragSpeed(0.01f), Format("%.3g")) \
    FEM_REGISTER_PROPERTY(TypeName, frequency_hz_, DisplayName("Frequency (Hz)"), SHOW_WHEN_MEMBER(TypeName, time_varying_, val), DragSpeed(0.01f), ClampMin(0.0f), Format("%.3g")) \
    FEM_REGISTER_PROPERTY(TypeName, phase_rad_, DisplayName("Phase (rad)"), SHOW_WHEN_MEMBER(TypeName, time_varying_, val), DragSpeed(0.01f), Format("%.3g")) \
    FEM_REGISTER_PROPERTY(TypeName, offset_, DisplayName("Offset"), SHOW_WHEN_MEMBER(TypeName, time_varying_, val), DragSpeed(0.01f), Format("%.3g")) \


#define FEM_DEFINE_PDE_SCALAR_PARAMETER(TypeName, ...)                      \
    FEM_DEFINE_OBJECT(TypeName, PDEScalarParameter, NoTypeHeader())         \
    FEM_BEGIN_PROPERTY_REGISTER(TypeName)                                   \
    {                                                                       \
        FEM_REGISTER_PROPERTY(TypeName, value_, __VA_ARGS__)                \
    }                                                                       \
    FEM_END_PROPERTY_REGISTER(TypeName)                                     \
    FEM_DEFINE_OBJECT(TypeName##Dynamic, TypeName, NoTypeHeader())          \
    FEM_BEGIN_PROPERTY_REGISTER(TypeName##Dynamic)                          \
    {                                                                       \
        FEM_REGISTER_PROPERTY(TypeName##Dynamic, value_, __VA_ARGS__)       \
        FEM_REGISTER_DYNAMIC_PARAMETER_CONFIG_PROPERTIES(TypeName##Dynamic) \
    }                                                                       \
    FEM_END_PROPERTY_REGISTER(TypeName##Dynamic)                            \
    void TypeName::apply(DifferentialEquation& equation) const {            \
        (equation.*s_member_).set_constant(this->value(equation.time));     \
    }                                                                       \
    double TypeName##Dynamic::value(double t) const {                       \
        if (!time_varying_)                                                 \
            return value_;                                                  \
        return calc_dynamic_value(t, value_);                               \
    }

#define FEM_DECLARE_PDE_SCALAR_CONSTANT(TypeName, Value)                    \
    class TypeName : public PDEScalarParameter {                            \
    public:                                                                 \
        FEM_DECLARE_OBJECT(TypeName);                                       \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName);                            \
        TypeName() {                                                        \
            value_ = Value;                                                 \
        }                                                                   \
    };

#define FEM_DEFINE_PDE_SCALAR_CONSTANT(TypeName, ...)                       \
    FEM_DEFINE_OBJECT(TypeName, PDEScalarParameter, NoTypeHeader())         \
    FEM_BEGIN_PROPERTY_REGISTER(TypeName)                                   \
    {                                                                       \
        FEM_REGISTER_PROPERTY(TypeName, value_, __VA_ARGS__)                \
    }                                                                       \
    FEM_END_PROPERTY_REGISTER(TypeName)
}

#endif // FEM_PDE_PARAMETER_H