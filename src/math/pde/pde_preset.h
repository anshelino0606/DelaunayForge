#ifndef FEM_PDE_PRESET_H
#define FEM_PDE_PRESET_H

#include "parameters/pde_rhs.h"
#include "parameters/pde_fractional_operator.h"
#include "core/object/object.h"
#include "core/object/property.h"
#include "core/macro.h"
#include <functional>
#include <concepts>
#include "math/fem/fem_builder.h"
#include "math/fem/fem_assembler.h"
#include "math/fem/field/fem_reference_provider.h"
#include "math/differential_equation.h"
#include "math/differential_equation_solution.h"
#include "math/pde/solve_request.h"

namespace fem {

struct FEMProblem;

using FEMAssembler = fem::FEMSystem (*)(const FEMProblem&, DifferentialEquationSolution&);

template<typename T, typename Real>
concept FEMIntegrator = requires(T integrator, const FEMMesh& mesh, const FEMMesh::Elem& E, Real Ke[3][3], Real be[3]) {
    { integrator.element(mesh, E, Ke, be) } -> std::same_as<void>;
};

struct PDEPresetAssemblers {
    FEMAssembler fem_assembler = nullptr;
};

enum class PDEPresetFlag : uint64_t {
    None = 0,
    NoRHS = 1 << 0,
    HasExactSolution = 1 << 1
};

[[nodiscard]] constexpr PDEPresetFlag operator|(PDEPresetFlag a, PDEPresetFlag b) noexcept {
    return static_cast<PDEPresetFlag>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

[[nodiscard]] constexpr PDEPresetFlag operator&(PDEPresetFlag a, PDEPresetFlag b) noexcept {
    return static_cast<PDEPresetFlag>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}

[[nodiscard]] constexpr bool has_flag(PDEPresetFlag flags, PDEPresetFlag check) noexcept {
    return (flags & check) == check;
}

using PDEPresetFlagBits = PDEPresetFlag;

template<typename T>
struct PDEPresetConfig {
    static constexpr bool use_rhs = true;
    static constexpr bool has_exact_solution = false;
};

enum class PDEPresetType : uint8_t {
    Undefined,
    Stationary,
    Transient
};

class PDEPreset : public Object {
public:
    FEM_DECLARE_OBJECT(PDEPreset);
    FEM_DECLARE_PROPERTY_REGISTER(PDEPreset);

    using ForEachParameter = std::function<void(PDEParameter*)>;

    virtual void apply(DifferentialEquation& equation) const {}
    virtual void for_each_parameter(const ForEachParameter& callback) const {}

    [[nodiscard]] virtual OperatorSpec operator_spec([[maybe_unused]] const DifferentialEquation& equation) const {
        OperatorSpec spec = LocalEllipticSpec{};
        for_each_parameter([&](PDEParameter* parameter) {
            if (const auto* fractional = dynamic_cast<const PDEParameters::FractionalOperator*>(parameter)) {
                spec = fractional->operator_spec();
            }
        });
        return spec;
    }

    [[nodiscard]] virtual SolveRequest make_solve_request(const DifferentialEquation& equation) const {
        return SolveRequest{.model = PDEModel(equation), .operator_spec = operator_spec(equation), .discretization = {}};
    }

    // Legacy facade: existing project code keeps calling this name.
    [[nodiscard]] virtual FEMAssembler fem_assembler() const { return nullptr; }

    [[nodiscard]] static PDEPreset* default_preset();

    [[nodiscard]] virtual bool has_exact_solution() const { return false; }

    [[nodiscard]] virtual bool evaluate_exact_solution(
        [[maybe_unused]] double x, 
        [[maybe_unused]] double y, 
        [[maybe_unused]] double& u_exact,
        [[maybe_unused]] double* ux_exact = nullptr, 
        [[maybe_unused]] double* uy_exact = nullptr
    ) const {
        return false;
    }

    virtual const IReferenceProvider* reference_provider() const { return nullptr; }

    [[nodiscard]] virtual bool has_initial_condition() const { return false; }
    [[nodiscard]] virtual double evaluate_initial_condition([[maybe_unused]] double x, [[maybe_unused]] double y) const {
        return 0.0;
    }

    [[nodiscard]] virtual bool has_initial_velocity() const { return false; }
    [[nodiscard]] virtual double evaluate_initial_velocity([[maybe_unused]] double x, [[maybe_unused]] double y) const {
        return 0.0;
    }

    [[nodiscard]] virtual PDEPresetType type() const { return PDEPresetType::Undefined; }

    [[nodiscard]] bool is_stationary() const { return type() == PDEPresetType::Stationary; }

protected:
    virtual void apply_custom(DifferentialEquation& equation) const {}
};

template<typename PDE>
bool evaluate_exact_solution(double x, double y, double& u_exact, double* ux_exact = nullptr, double* uy_exact = nullptr) {
    return false;
}

template<typename PDE>
class TPDEPreset_Base {
public:
    using Config = PDEPresetConfig<PDE>;
    static constexpr bool use_rhs = Config::use_rhs;

    [[nodiscard]] virtual bool has_exact_solution() const { return Config::has_exact_solution; }

    [[nodiscard]] virtual bool evaluate_exact_solution(
        [[maybe_unused]] double x, 
        [[maybe_unused]] double y, 
        [[maybe_unused]] double& u_exact,
        [[maybe_unused]] double* ux_exact = nullptr, 
        [[maybe_unused]] double* uy_exact = nullptr
    ) const {
        if constexpr (Config::has_exact_solution) {
            return fem::evaluate_exact_solution<PDE>(x, y, u_exact, ux_exact, uy_exact);
        }

        return false;
    }
};

template<typename PDE>
class TPDEPreset_DefaultStationary : public PDEPreset, public TPDEPreset_Base<PDE> {
    using Base = TPDEPreset_Base<PDE>;

public:
    TPDEPreset_DefaultStationary() {
        if constexpr (Base::use_rhs) {
            rhs_ = PDE_RHS::default_rhs();
        }
    }

    ~TPDEPreset_DefaultStationary() {
        if constexpr (Base::use_rhs) {
            if (rhs_) {
                destroy_object(rhs_);
            }
        }
    }

    [[nodiscard]] virtual PDEPresetType type() const override { return PDEPresetType::Stationary; }

protected:
    PDE_RHS* rhs_ = nullptr;

    void apply_rhs(DifferentialEquation& equation) {
        if constexpr (Base::use_rhs) {
            if (rhs_) {
                rhs_->apply(equation);
            }
        }
    }
};

template<typename PDE>
class TPDEPreset_TransientBase : public TPDEPreset_Base<PDE> {
    using Base = TPDEPreset_Base<PDE>;

public:
    TPDEPreset_TransientBase() {
        if constexpr (Base::use_rhs) {
            rhs_ = PDEDynamicRHS::default_rhs();
        }
    }

    ~TPDEPreset_TransientBase() {
        if constexpr (Base::use_rhs) {
            if (rhs_) {
                destroy_object(rhs_);
            }
        }
    }
    
protected:
    PDEDynamicRHS* rhs_ = nullptr;

    void apply_rhs(DifferentialEquation& equation) {
        if constexpr (Base::use_rhs) {
            if (rhs_) {
                rhs_->apply(equation);
            }
        }
    }
};

template<typename PDE, typename InitParam>
class TPDEPreset_DefaultTransient_1IC : public PDEPreset, public TPDEPreset_TransientBase<PDE> {
public:
    TPDEPreset_DefaultTransient_1IC() {
        init_param_ = create_object<InitParam>();
    }

    ~TPDEPreset_DefaultTransient_1IC() {
        if (init_param_) {
            destroy_object(init_param_);
        }
    }
    
    [[nodiscard]] virtual bool has_initial_condition() const override { return true; }
    [[nodiscard]] virtual double evaluate_initial_condition([[maybe_unused]] double x, [[maybe_unused]] double y) const override {
        return init_param_->value();
    }

    [[nodiscard]] virtual PDEPresetType type() const override { return PDEPresetType::Transient; }

protected:
    InitParam* init_param_ = nullptr;
};

template<typename PDE, typename InitUParam, typename InitVParam>
class TPDEPreset_DefaultTransient_2IC : public PDEPreset, public TPDEPreset_TransientBase<PDE> {
public:
    TPDEPreset_DefaultTransient_2IC() {
        init_u_param_ = create_object<InitUParam>();
        init_v_param_ = create_object<InitVParam>();
    }

    ~TPDEPreset_DefaultTransient_2IC() {
        if (init_u_param_) {
            destroy_object(init_u_param_);
        }
        if (init_v_param_) {
            destroy_object(init_v_param_);
        }
    }

    [[nodiscard]] virtual bool has_initial_condition() const override { return true; }
    [[nodiscard]] virtual double evaluate_initial_condition([[maybe_unused]] double x, [[maybe_unused]] double y) const override {
        return init_u_param_->value();
    }

    [[nodiscard]] virtual bool has_initial_velocity() const override { return false; }
    [[nodiscard]] virtual double evaluate_initial_velocity([[maybe_unused]] double x, [[maybe_unused]] double y) const override {
        return init_v_param_->value();
    }

protected:
    InitUParam* init_u_param_ = nullptr;
    InitVParam* init_v_param_ = nullptr;
};

#define ADD_PDE_PRESET_PARAM(ParamType)                                                     \
    protected:                                                                              \
        PDEParameters::ParamType* ParamType;                                                \
    public:                                                                                 \
        PDEParameters::ParamType* get_##ParamType() const {                                 \
            return ParamType;                                                               \
        }

#define CREATE_PDE_PRESET_PARAM(ParamType)                                                  \
    ParamType = create_object<PDEParameters::ParamType>();

#define DESTROY_PDE_PRESET_PARAM(ParamType)                                                 \
    destroy_object(ParamType);

#define APPLY_PDE_PARAM(ParamType)                                                          \
    ParamType->apply(equation);    
    
#define CALLBACK_FOR_EACH_PDE_PARAM(ParamType)                                              \
    callback(ParamType);

#define REGISTER_PDE_PARAM(TypeName, ParamName)                                             \
    FEM_REGISTER_PROPERTY(TypeName, ParamName, NoTypeHeader())

#define ADD_PDE_PRESET_DYNAMIC_PARAM(ParamType) ADD_PDE_PRESET_PARAM(ParamType##Dynamic)
#define CREATE_PDE_PRESET_DYNAMIC_PARAM(ParamType) CREATE_PDE_PRESET_PARAM(ParamType##Dynamic)
#define DESTROY_PDE_PRESET_DYNAMIC_PARAM(ParamType) DESTROY_PDE_PRESET_PARAM(ParamType##Dynamic)
#define APPLY_PDE_DYNAMIC_PARAM(ParamType) APPLY_PDE_PARAM(ParamType##Dynamic)
#define CALLBACK_FOR_EACH_PDE_DYNAMIC_PARAM(ParamType) CALLBACK_FOR_EACH_PDE_PARAM(ParamType##Dynamic)
#define REGISTER_PDE_DYNAMIC_PARAM(TypeName, ParamType) REGISTER_PDE_PARAM(TypeName, ParamType##Dynamic)

#define REGISTER_RHS(TypeName)                                                              \
    if constexpr (PDEPresetConfig<TypeName>::use_rhs) {\
        FEM_REGISTER_PROPERTY(TypeName, rhs_, DisplayName("RHS Kind"), BaseClass(), NoTypeHeader())\
    }

#define DECLARE_PDE_PRESET_INTERNAL(TypeName, BaseClass, AddParamMacro, CreateParamMacro, DestroyParamMacro, ApplyParamMacro, ParamCallbackMacro, ...) \
    class TypeName : public BaseClass {                                                     \
    public:                                                                                 \
        using Config = PDEPresetConfig<TypeName>;                                           \
        FEM_DECLARE_OBJECT(TypeName);                                                       \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName);                                            \
        FEM_FOR_EACH(AddParamMacro, __VA_ARGS__)                                            \
        TypeName() {                                                                        \
            FEM_FOR_EACH(CreateParamMacro, __VA_ARGS__)                                     \
        }                                                                                   \
        virtual ~TypeName() override {                                                      \
            FEM_FOR_EACH(DestroyParamMacro, __VA_ARGS__)                                    \
        }                                                                                   \
        virtual void apply(DifferentialEquation& equation) const override {                 \
            FEM_FOR_EACH(ApplyParamMacro, __VA_ARGS__)                                      \
            apply_custom(equation);                                                         \
            if (rhs_) rhs_->apply(equation);                                                \
        }                                                                                   \
        virtual void for_each_parameter(const ForEachParameter& callback) const override {  \
            FEM_FOR_EACH(ParamCallbackMacro, __VA_ARGS__)                                   \
        }                                                                                   \
        virtual FEMAssembler fem_assembler() const override;                                \
    };

#define FEM_DECLARE_STATIONARY_PDE_PRESET(TypeName, ParentClass, ...)                       \
    DECLARE_PDE_PRESET_INTERNAL(TypeName, ParentClass, ADD_PDE_PRESET_PARAM, CREATE_PDE_PRESET_PARAM, DESTROY_PDE_PRESET_PARAM, APPLY_PDE_PARAM, CALLBACK_FOR_EACH_PDE_PARAM, __VA_ARGS__)\
    inline FEM_BEGIN_PROPERTY_REGISTER(TypeName) {                                          \
        FEM_FOR_EACH_CONTEXT(REGISTER_PDE_PARAM, TypeName, __VA_ARGS__)                     \
        REGISTER_RHS(TypeName)                                                              \
    }

#define FEM_DECLARE_DEFAULT_STATIONARY_PDE_PRESET(TypeName, ...)                            \
    FEM_DECLARE_STATIONARY_PDE_PRESET(TypeName, TPDEPreset_DefaultStationary<TypeName>, __VA_ARGS__)

#define FEM_DECLARE_TRANSIENT_PDE_PRESET_1IC(TypeName, ParentClass, InitParam, ...)         \
    class TypeName;                                                                         \
    using TypeName##Base = ParentClass<TypeName, InitParam>;                                \
    DECLARE_PDE_PRESET_INTERNAL(TypeName, TypeName##Base, ADD_PDE_PRESET_DYNAMIC_PARAM, CREATE_PDE_PRESET_DYNAMIC_PARAM, DESTROY_PDE_PRESET_DYNAMIC_PARAM, APPLY_PDE_DYNAMIC_PARAM, CALLBACK_FOR_EACH_PDE_DYNAMIC_PARAM, __VA_ARGS__)\
    inline FEM_BEGIN_PROPERTY_REGISTER(TypeName) {                                          \
        FEM_FOR_EACH_CONTEXT(REGISTER_PDE_DYNAMIC_PARAM, TypeName, __VA_ARGS__)             \
        FEM_REGISTER_PROPERTY(TypeName, init_param_, NoTypeHeader())                        \
        REGISTER_RHS(TypeName)                                                              \
    }

#define FEM_DECLARE_TRANSIENT_PDE_PRESET_2IC(TypeName, ParentClass, InitUParam, InitVParam, ...)\
    class TypeName;                                                                         \
    using TypeName##Base = ParentClass<TypeName, InitUParam, InitVParam>;                   \
    DECLARE_PDE_PRESET_INTERNAL(TypeName, TypeName##Base, ADD_PDE_PRESET_DYNAMIC_PARAM, CREATE_PDE_PRESET_DYNAMIC_PARAM, DESTROY_PDE_PRESET_DYNAMIC_PARAM, APPLY_PDE_DYNAMIC_PARAM, CALLBACK_FOR_EACH_PDE_DYNAMIC_PARAM, __VA_ARGS__)\
    inline FEM_BEGIN_PROPERTY_REGISTER(TypeName) {                                          \
        FEM_REGISTER_PROPERTY(TypeName, init_u_param_, NoTypeHeader())                      \
        FEM_REGISTER_PROPERTY(TypeName, init_v_param_, NoTypeHeader())                      \
        FEM_FOR_EACH_CONTEXT(REGISTER_PDE_DYNAMIC_PARAM, TypeName, __VA_ARGS__)             \
        REGISTER_RHS(TypeName)                                                              \
    }

#define FEM_DECLARE_DEFAULT_TRANSIENT_PRESET_1IC(TypeName, InitParam, ...)                  \
    FEM_DECLARE_TRANSIENT_PDE_PRESET_1IC(TypeName, TPDEPreset_DefaultTransient_1IC, InitParam, __VA_ARGS__)

#define FEM_DECLARE_DEFAULT_TRANSIENT_PRESET_2IC(TypeName, InitUParam, InitVParam, ...)     \
    FEM_DECLARE_TRANSIENT_PDE_PRESET_2IC(TypeName, TPDEPreset_DefaultTransient_2IC, InitUParam, InitVParam, __VA_ARGS__)

#define FEM_DECLARE_PDE_PRESET(TypeName, ...)                                               \
    class TypeName : public PDEPreset {                                                     \
    public:                                                                                 \
        FEM_DECLARE_OBJECT(TypeName);                                                       \
        FEM_DECLARE_PROPERTY_REGISTER(TypeName);                                            \
        FEM_FOR_EACH(ADD_PDE_PRESET_PARAM, __VA_ARGS__)                                     \
        TypeName();                                                                         \
        virtual ~TypeName() override;                                                       \
        virtual void apply(DifferentialEquation& equation) const override;                  \
        virtual void for_each_parameter(const ForEachParameter& callback) const override {  \
            FEM_FOR_EACH(CALLBACK_FOR_EACH_PDE_PARAM, __VA_ARGS__)                          \
        }                                                                                   \
        virtual FEMAssembler fem_assembler() const override;                                \
        virtual bool has_exact_solution() const override;                                   \
        virtual bool evaluate_exact_solution(double x, double y, double& u_exact,           \
                                            double* ux_exact = nullptr,                     \
                                            double* uy_exact = nullptr) const override;     \
    };

#define REGISTER_PDE_PRESET_PARAM(TypeName, ParamName)                                      \
    FEM_REGISTER_PROPERTY(TypeName, ParamName, NoTypeHeader())

#define FEM_DEFINE_PDE_PRESET(TypeName, Name, Assemblers, ...)                                              \
    FEM_DEFINE_OBJECT(TypeName, PDEPreset, DisplayName(Name))                                               \
    FEM_END_PROPERTY_REGISTER(TypeName)                                                                     \
    static const PDEPresetAssemblers TypeName##_Assemblers = Assemblers;                                    \
    FEMAssembler TypeName::fem_assembler() const { return TypeName##_Assemblers.fem_assembler; }

#define FEM_DEFINE_PDE_PRESET_CONFIG(TypeName, BitMask)                                                     \
    class TypeName;                                                                                         \
    template<>                                                                                              \
    struct PDEPresetConfig<TypeName> {                                                                      \
        static constexpr bool use_rhs = !has_flag(BitMask, PDEPresetFlag::NoRHS);                           \
        static constexpr bool has_exact_solution = has_flag(BitMask, PDEPresetFlag::HasExactSolution);      \
    };

}

#endif // FEM_PDE_PRESET_H