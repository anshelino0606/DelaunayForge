#ifndef FEM_PDE_RHS_H
#define FEM_PDE_RHS_H

#include "pde_parameter.h"

namespace fem {

class PDE_RHS : public PDEParameter {
public:
    FEM_DECLARE_OBJECT(PDE_RHS);
    FEM_DECLARE_PROPERTY_REGISTER(PDE_RHS);

    static PDE_RHS* default_rhs();
};

class PDEDynamicRHS : public PDEParameter {
public:
    FEM_DECLARE_OBJECT(PDEDynamicRHS);
    FEM_DECLARE_PROPERTY_REGISTER(PDEDynamicRHS);

    static PDEDynamicRHS* default_rhs();
};

#define FEM_DECLARE_RHS_SCALAR_PARAMETER(TypeName)                              \
    namespace RHSParameters {                                                   \
        class TypeName : public PDEScalarParameter {                            \
        public:                                                                 \
            FEM_DECLARE_OBJECT(TypeName);                                       \
            FEM_DECLARE_PROPERTY_REGISTER(TypeName);                            \
        };                                                                      \
        class TypeName##Dynamic                                                 \
            : public TypeName, public PDEDynamicParameterConfig {               \
        public:                                                                 \
            FEM_DECLARE_OBJECT(TypeName##Dynamic);                              \
            FEM_DECLARE_PROPERTY_REGISTER(TypeName##Dynamic);                   \
        };}

#define FEM_DECLARE_RHS_SCALAR_PARAMETER_DEFAULT_VALUE(TypeName, Value)         \
    namespace RHSParameters {                                                   \
        class TypeName : public PDEScalarParameter {                            \
        public:                                                                 \
            FEM_DECLARE_OBJECT(TypeName);                                       \
            FEM_DECLARE_PROPERTY_REGISTER(TypeName);                            \
            TypeName() {                                                        \
                value_ = Value;                                                 \
            }                                                                   \
        };                                                                      \
        class TypeName##Dynamic                                                 \
            : public TypeName, public PDEDynamicParameterConfig {               \
        public:                                                                 \
            FEM_DECLARE_OBJECT(TypeName##Dynamic);                              \
            FEM_DECLARE_PROPERTY_REGISTER(TypeName##Dynamic);                   \
            TypeName##Dynamic() {                                               \
                value_ = Value;                                                 \
            }                                                                   \
        };}

#define FEM_DEFINE_RHS_SCALAR_PARAMETER(TypeName, ...)                          \
    FEM_DEFINE_OBJECT(TypeName, PDEParameter, NoTypeHeader())                   \
    FEM_BEGIN_PROPERTY_REGISTER(TypeName)                                       \
    {                                                                           \
        FEM_REGISTER_PROPERTY(TypeName, value_, __VA_ARGS__)                    \
    }                                                                           \
    FEM_END_PROPERTY_REGISTER(TypeName)                                         \
    FEM_DEFINE_OBJECT(TypeName##Dynamic, TypeName, NoTypeHeader())              \
    FEM_BEGIN_PROPERTY_REGISTER(TypeName##Dynamic)                              \
    {                                                                           \
        FEM_REGISTER_PROPERTY(TypeName##Dynamic, value_, __VA_ARGS__)           \
        FEM_REGISTER_DYNAMIC_PARAMETER_CONFIG_PROPERTIES(TypeName##Dynamic)     \
    }                                                                           \
    FEM_END_PROPERTY_REGISTER(TypeName##Dynamic)

#define CREATE_RHS_PARAM(ParamType)                                             \
    ParamType = create_object<RHSParameters::ParamType>();

#define DESTROY_RHS_PARAM(ParamType)                                            \
    destroy_object(ParamType);

#define ADD_RHS_PARAM(ParamType)                                                \
    protected:                                                                  \
        RHSParameters::ParamType* ParamType;                                    \
    public:                                                                     \
        RHSParameters::ParamType* get_##ParamType() const {                     \
            return ParamType;                                                   \
        }

#define ADD_RHS_DYNAMIC_PARAM(ParamType) ADD_RHS_PARAM(ParamType##Dynamic)
#define CREATE_RHS_DYNAMIC_PARAM(ParamType) CREATE_RHS_PARAM(ParamType##Dynamic)
#define DESTROY_RHS_DYNAMIC_PARAM(ParamType) DESTROY_RHS_PARAM(ParamType##Dynamic)

#define FEM_DECLARE_RHS(TypeName, ...)                                          \
    class TypeName : public PDE_RHS {                                           \
    public:                                                                     \
        FEM_DECLARE_OBJECT(TypeName);                                           \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName);                                \
        FEM_FOR_EACH(ADD_RHS_PARAM, __VA_ARGS__)                                \
        TypeName() {                                                            \
            FEM_FOR_EACH(CREATE_RHS_PARAM, __VA_ARGS__)                         \
        }                                                                       \
        virtual ~TypeName() override {                                          \
            FEM_FOR_EACH(DESTROY_RHS_PARAM, __VA_ARGS__)                        \
        }                                                                       \
        virtual void apply(DifferentialEquation& equation) const override;      \
    };                                                                          \
    class TypeName##Dynamic : public PDEDynamicRHS {                            \
    public:                                                                     \
        FEM_DECLARE_OBJECT(TypeName##Dynamic);                                  \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName##Dynamic);                       \
        FEM_FOR_EACH(ADD_RHS_DYNAMIC_PARAM, __VA_ARGS__)                        \
        TypeName##Dynamic() {                                                   \
            FEM_FOR_EACH(CREATE_RHS_DYNAMIC_PARAM, __VA_ARGS__)                 \
        }                                                                       \
        virtual ~TypeName##Dynamic() override {                                 \
            FEM_FOR_EACH(DESTROY_RHS_DYNAMIC_PARAM, __VA_ARGS__)                \
        }                                                                       \
        virtual void apply(DifferentialEquation& equation) const override;      \
    };

#define REGISTER_RHS_PARAM(TypeName, ParamName)                                 \
    FEM_REGISTER_PROPERTY(TypeName, ParamName, NoTypeHeader())

#define REGISTER_RHS_DYNAMIC_PARAM(TypeName, ParamName)                         \
    REGISTER_RHS_PARAM(TypeName, ParamName##Dynamic)

#define DECLARE_RHS_PARAMS_CONTEXT_FIELD(ParamName)                             \
    ParamName* ParamName##_;

#define SET_RHS_PARAMS_CONTEXT_FIELD(ParamName)                                 \
    .ParamName##_ = get_##ParamName(),

#define SET_RHS_DYNAMIC_PARAMS_CONTEXT_FIELD(ParamName)                         \
    .ParamName##_ = get_##ParamName##Dynamic(),

#define FEM_DEFINE_RHS(TypeName, Name, ...)                                     \
    FEM_DEFINE_OBJECT(TypeName, PDE_RHS, DisplayName(Name))                     \
    FEM_BEGIN_PROPERTY_REGISTER(TypeName)                                       \
    {                                                                           \
        FEM_FOR_EACH_CONTEXT(REGISTER_RHS_PARAM, TypeName, __VA_ARGS__)         \
    }                                                                           \
    FEM_END_PROPERTY_REGISTER(TypeName)                                         \
    FEM_DEFINE_OBJECT(TypeName##Dynamic, PDEDynamicRHS, DisplayName(Name))      \
    FEM_BEGIN_PROPERTY_REGISTER(TypeName##Dynamic)                              \
    {                                                                           \
        FEM_FOR_EACH_CONTEXT(REGISTER_RHS_DYNAMIC_PARAM, TypeName##Dynamic, __VA_ARGS__)\
    }                                                                           \
    FEM_END_PROPERTY_REGISTER(TypeName##Dynamic)                                \
    struct TypeName##Params {                                                   \
        FEM_FOR_EACH(DECLARE_RHS_PARAMS_CONTEXT_FIELD, __VA_ARGS__)             \
    };                                                                          \
    void apply(DifferentialEquation& equation, const TypeName##Params& ctx);    \
    void TypeName::apply(DifferentialEquation& equation) const {                \
        fem::apply(equation, TypeName##Params{                                  \
            FEM_FOR_EACH(SET_RHS_PARAMS_CONTEXT_FIELD, __VA_ARGS__)});          \
    }                                                                           \
    void TypeName##Dynamic::apply(DifferentialEquation& equation) const {       \
        fem::apply(equation, TypeName##Params{                                  \
            FEM_FOR_EACH(SET_RHS_DYNAMIC_PARAMS_CONTEXT_FIELD, __VA_ARGS__)});  \
    }



}

#endif // FEM_PDE_RHS_H