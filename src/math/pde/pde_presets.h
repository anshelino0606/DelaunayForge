#ifndef FEM_PDE_PRESETS_H
#define FEM_PDE_PRESETS_H

#include "pde_preset.h"
#include "parameters/pde_coefficients.h"
#include "parameters/pde_constants.h"
#include "parameters/pde_fractional_operator.h"

namespace fem {

class PDEPreset_Laplace final : public PDEPreset {
public:
    FEM_DECLARE_OBJECT(PDEPreset_Laplace);
    FEM_DECLARE_PROPERTY_REGISTER(PDEPreset_Laplace);

    PDEPreset_Laplace();
    ~PDEPreset_Laplace() override;

    [[nodiscard]] PDEParameterBundleView parameter_bundle() const override;
    void apply(DifferentialEquation& equation) const override;
    void for_each_parameter(const ForEachParameter& callback) const override;
    [[nodiscard]] SolveKind solve_kind() const override { return SolveKind::Stationary; }

protected:
    PDEParameters::Diffusivity* Diffusivity = nullptr;
};

class PDEPreset_Poisson final : public PDEPreset {
public:
    FEM_DECLARE_OBJECT(PDEPreset_Poisson);
    FEM_DECLARE_PROPERTY_REGISTER(PDEPreset_Poisson);

    PDEPreset_Poisson();
    ~PDEPreset_Poisson() override;

    [[nodiscard]] PDEParameterBundleView parameter_bundle() const override;
    void apply(DifferentialEquation& equation) const override;
    void for_each_parameter(const ForEachParameter& callback) const override;
    [[nodiscard]] SolveKind solve_kind() const override { return SolveKind::Stationary; }
    [[nodiscard]] bool has_exact_solution() const override { return true; }
    [[nodiscard]] bool evaluate_exact_solution(double x, double y, double& u_exact, double* ux_exact = nullptr, double* uy_exact = nullptr) const override;

protected:
    PDEParameters::Diffusivity* Diffusivity = nullptr;
    PDE_RHS* rhs_ = nullptr;
};

class PDEPreset_Reaction final : public PDEPreset {
public:
    FEM_DECLARE_OBJECT(PDEPreset_Reaction);
    FEM_DECLARE_PROPERTY_REGISTER(PDEPreset_Reaction);

    PDEPreset_Reaction();
    ~PDEPreset_Reaction() override;

    [[nodiscard]] PDEParameterBundleView parameter_bundle() const override;
    void apply(DifferentialEquation& equation) const override;
    void for_each_parameter(const ForEachParameter& callback) const override;
    [[nodiscard]] SolveKind solve_kind() const override { return SolveKind::Stationary; }

protected:
    PDEParameters::Diffusivity* Diffusivity = nullptr;
    PDEParameters::Reaction* Reaction = nullptr;
    PDE_RHS* rhs_ = nullptr;
};

class PDEPreset_Fractional final : public PDEPreset {
public:
    FEM_DECLARE_OBJECT(PDEPreset_Fractional);
    FEM_DECLARE_PROPERTY_REGISTER(PDEPreset_Fractional);

    PDEPreset_Fractional();
    ~PDEPreset_Fractional() override;

    [[nodiscard]] PDEParameterBundleView parameter_bundle() const override;
    void apply(DifferentialEquation& equation) const override;
    void for_each_parameter(const ForEachParameter& callback) const override;
    [[nodiscard]] SolveKind solve_kind() const override { return SolveKind::Stationary; }

protected:
    PDEParameters::FractionalOperator* FractionalOperator = nullptr;
    PDE_RHS* rhs_ = nullptr;
};

class PDEPreset_Heat final : public PDEPreset {
public:
    FEM_DECLARE_OBJECT(PDEPreset_Heat);
    FEM_DECLARE_PROPERTY_REGISTER(PDEPreset_Heat);

    PDEPreset_Heat();
    ~PDEPreset_Heat() override;

    [[nodiscard]] PDEParameterBundleView parameter_bundle() const override;
    void apply(DifferentialEquation& equation) const override;
    void for_each_parameter(const ForEachParameter& callback) const override;
    [[nodiscard]] SolveKind solve_kind() const override { return SolveKind::HeatImplicitEuler; }
    [[nodiscard]] bool has_initial_condition() const override { return true; }
    [[nodiscard]] double evaluate_initial_condition(double x, double y) const override;

protected:
    Temperature* init_param_ = nullptr;
    PDEParameters::DiffusivityDynamic* DiffusivityDynamic = nullptr;
    PDEParameters::ReactionDynamic* ReactionDynamic = nullptr;
    PDEDynamicRHS* rhs_ = nullptr;
};

class PDEPreset_Wave final : public PDEPreset {
public:
    FEM_DECLARE_OBJECT(PDEPreset_Wave);
    FEM_DECLARE_PROPERTY_REGISTER(PDEPreset_Wave);

    PDEPreset_Wave();
    ~PDEPreset_Wave() override;

    [[nodiscard]] PDEParameterBundleView parameter_bundle() const override;
    void apply(DifferentialEquation& equation) const override;
    void for_each_parameter(const ForEachParameter& callback) const override;
    [[nodiscard]] SolveKind solve_kind() const override { return SolveKind::WaveNewmark; }
    [[nodiscard]] bool has_initial_condition() const override { return true; }
    [[nodiscard]] double evaluate_initial_condition(double x, double y) const override;
    [[nodiscard]] bool has_initial_velocity() const override { return true; }
    [[nodiscard]] double evaluate_initial_velocity(double x, double y) const override;

protected:
    Displacement* init_u_param_ = nullptr;
    Velocity* init_v_param_ = nullptr;
    PDEParameters::DiffusivityDynamic* DiffusivityDynamic = nullptr;
    PDEParameters::ReactionDynamic* ReactionDynamic = nullptr;
};

} // namespace fem

#endif // FEM_PDE_PRESETS_H
