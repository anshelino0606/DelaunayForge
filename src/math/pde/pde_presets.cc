#include "pde_presets.h"
#include <cmath>

namespace fem {

FEM_DEFINE_OBJECT(PDEPreset_Laplace, PDEPreset, DisplayName("Laplace (-u = 0)"));
FEM_BEGIN_PROPERTY_REGISTER(PDEPreset_Laplace)
{
    FEM_REGISTER_PROPERTY(PDEPreset_Laplace, Diffusivity, NoTypeHeader());
}
FEM_END_PROPERTY_REGISTER(PDEPreset_Laplace);

PDEPreset_Laplace::PDEPreset_Laplace() {
    Diffusivity = create_object<PDEParameters::Diffusivity>();
}
PDEPreset_Laplace::~PDEPreset_Laplace() {
    if (Diffusivity) destroy_object(Diffusivity);
}
PDEParameterBundleView PDEPreset_Laplace::parameter_bundle() const {
    PDEParameterBundleView bundle;
    bundle.add(Diffusivity);
    return bundle;
}
void PDEPreset_Laplace::apply(DifferentialEquation& equation) const {
    parameter_bundle().apply(equation);
    apply_custom(equation);
    equation.f.set_constant(0.0);
}
void PDEPreset_Laplace::for_each_parameter(const ForEachParameter& callback) const {
    parameter_bundle().for_each(callback);
}

FEM_DEFINE_OBJECT(PDEPreset_Poisson, PDEPreset, DisplayName("Poisson (-u = f)"));
FEM_BEGIN_PROPERTY_REGISTER(PDEPreset_Poisson)
{
    FEM_REGISTER_PROPERTY(PDEPreset_Poisson, Diffusivity, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Poisson, rhs_, DisplayName("RHS Kind"), BaseClass(), NoTypeHeader());
}
FEM_END_PROPERTY_REGISTER(PDEPreset_Poisson);

PDEPreset_Poisson::PDEPreset_Poisson() {
    Diffusivity = create_object<PDEParameters::Diffusivity>();
    rhs_ = PDE_RHS::default_rhs();
}
PDEPreset_Poisson::~PDEPreset_Poisson() {
    if (Diffusivity) destroy_object(Diffusivity);
    if (rhs_) destroy_object(rhs_);
}
PDEParameterBundleView PDEPreset_Poisson::parameter_bundle() const {
    PDEParameterBundleView bundle;
    bundle.add(Diffusivity);
    return bundle;
}
void PDEPreset_Poisson::apply(DifferentialEquation& equation) const {
    parameter_bundle().apply(equation);
    apply_custom(equation);
    if (rhs_) rhs_->apply(equation);
}
void PDEPreset_Poisson::for_each_parameter(const ForEachParameter& callback) const {
    parameter_bundle().for_each(callback);
}
bool PDEPreset_Poisson::evaluate_exact_solution(double x, double y, double& u_exact, double* ux_exact, double* uy_exact) const {
    const double pi = M_PI;
    u_exact = std::sin(pi * x) * std::sin(pi * y);
    if (ux_exact) *ux_exact = pi * std::cos(pi * x) * std::sin(pi * y);
    if (uy_exact) *uy_exact = pi * std::sin(pi * x) * std::cos(pi * y);
    return true;
}

FEM_DEFINE_OBJECT(PDEPreset_Reaction, PDEPreset, DisplayName("Reaction (-u? + c u = f)"));
FEM_BEGIN_PROPERTY_REGISTER(PDEPreset_Reaction)
{
    FEM_REGISTER_PROPERTY(PDEPreset_Reaction, Diffusivity, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Reaction, Reaction, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Reaction, rhs_, DisplayName("RHS Kind"), BaseClass(), NoTypeHeader());
}
FEM_END_PROPERTY_REGISTER(PDEPreset_Reaction);

PDEPreset_Reaction::PDEPreset_Reaction() {
    Diffusivity = create_object<PDEParameters::Diffusivity>();
    Reaction = create_object<PDEParameters::Reaction>();
    rhs_ = PDE_RHS::default_rhs();
}
PDEPreset_Reaction::~PDEPreset_Reaction() {
    if (Diffusivity) destroy_object(Diffusivity);
    if (Reaction) destroy_object(Reaction);
    if (rhs_) destroy_object(rhs_);
}
PDEParameterBundleView PDEPreset_Reaction::parameter_bundle() const {
    PDEParameterBundleView bundle;
    bundle.add(Diffusivity);
    bundle.add(Reaction);
    return bundle;
}
void PDEPreset_Reaction::apply(DifferentialEquation& equation) const {
    parameter_bundle().apply(equation);
    apply_custom(equation);
    if (rhs_) rhs_->apply(equation);
}
void PDEPreset_Reaction::for_each_parameter(const ForEachParameter& callback) const {
    parameter_bundle().for_each(callback);
}

FEM_DEFINE_OBJECT(PDEPreset_Fractional, PDEPreset, DisplayName("Fractional ((-delta)^s u = f)"));
FEM_BEGIN_PROPERTY_REGISTER(PDEPreset_Fractional)
{
    FEM_REGISTER_PROPERTY(PDEPreset_Fractional, FractionalOperator, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Fractional, rhs_, DisplayName("RHS Kind"), BaseClass(), NoTypeHeader());
}
FEM_END_PROPERTY_REGISTER(PDEPreset_Fractional);

PDEPreset_Fractional::PDEPreset_Fractional() {
    FractionalOperator = create_object<PDEParameters::FractionalOperator>();
    rhs_ = PDE_RHS::default_rhs();
}
PDEPreset_Fractional::~PDEPreset_Fractional() {
    if (FractionalOperator) destroy_object(FractionalOperator);
    if (rhs_) destroy_object(rhs_);
}
PDEParameterBundleView PDEPreset_Fractional::parameter_bundle() const {
    PDEParameterBundleView bundle;
    bundle.add(FractionalOperator);
    return bundle;
}
void PDEPreset_Fractional::apply(DifferentialEquation& equation) const {
    parameter_bundle().apply(equation);
    apply_custom(equation);
    if (rhs_) rhs_->apply(equation);
}
void PDEPreset_Fractional::for_each_parameter(const ForEachParameter& callback) const {
    parameter_bundle().for_each(callback);
}

FEM_DEFINE_OBJECT(PDEPreset_Heat, PDEPreset, DisplayName("Heat (u_t - div(a grad u) + c u = f)"));
FEM_BEGIN_PROPERTY_REGISTER(PDEPreset_Heat)
{
    FEM_REGISTER_PROPERTY(PDEPreset_Heat, DiffusivityDynamic, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Heat, ReactionDynamic, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Heat, init_param_, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Heat, rhs_, DisplayName("RHS Kind"), BaseClass(), NoTypeHeader());
}
FEM_END_PROPERTY_REGISTER(PDEPreset_Heat);

PDEPreset_Heat::PDEPreset_Heat() {
    init_param_ = create_object<Temperature>();
    DiffusivityDynamic = create_object<PDEParameters::DiffusivityDynamic>();
    ReactionDynamic = create_object<PDEParameters::ReactionDynamic>();
    rhs_ = PDEDynamicRHS::default_rhs();
}
PDEPreset_Heat::~PDEPreset_Heat() {
    if (init_param_) destroy_object(init_param_);
    if (DiffusivityDynamic) destroy_object(DiffusivityDynamic);
    if (ReactionDynamic) destroy_object(ReactionDynamic);
    if (rhs_) destroy_object(rhs_);
}
PDEParameterBundleView PDEPreset_Heat::parameter_bundle() const {
    PDEParameterBundleView bundle;
    bundle.add(DiffusivityDynamic);
    bundle.add(ReactionDynamic);
    return bundle;
}
void PDEPreset_Heat::apply(DifferentialEquation& equation) const {
    parameter_bundle().apply(equation);
    apply_custom(equation);
    if (rhs_) rhs_->apply(equation);
}
void PDEPreset_Heat::for_each_parameter(const ForEachParameter& callback) const {
    parameter_bundle().for_each(callback);
}
double PDEPreset_Heat::evaluate_initial_condition(double, double) const {
    return init_param_ ? init_param_->value() : 0.0;
}

FEM_DEFINE_OBJECT(PDEPreset_Wave, PDEPreset, DisplayName("Wave (u_tt - div(a grad u) + c u = f)"));
FEM_BEGIN_PROPERTY_REGISTER(PDEPreset_Wave)
{
    FEM_REGISTER_PROPERTY(PDEPreset_Wave, init_u_param_, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Wave, init_v_param_, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Wave, DiffusivityDynamic, NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEPreset_Wave, ReactionDynamic, NoTypeHeader());
}
FEM_END_PROPERTY_REGISTER(PDEPreset_Wave);

PDEPreset_Wave::PDEPreset_Wave() {
    init_u_param_ = create_object<Displacement>();
    init_v_param_ = create_object<Velocity>();
    DiffusivityDynamic = create_object<PDEParameters::DiffusivityDynamic>();
    ReactionDynamic = create_object<PDEParameters::ReactionDynamic>();
}
PDEPreset_Wave::~PDEPreset_Wave() {
    if (init_u_param_) destroy_object(init_u_param_);
    if (init_v_param_) destroy_object(init_v_param_);
    if (DiffusivityDynamic) destroy_object(DiffusivityDynamic);
    if (ReactionDynamic) destroy_object(ReactionDynamic);
}
PDEParameterBundleView PDEPreset_Wave::parameter_bundle() const {
    PDEParameterBundleView bundle;
    bundle.add(DiffusivityDynamic);
    bundle.add(ReactionDynamic);
    return bundle;
}
void PDEPreset_Wave::apply(DifferentialEquation& equation) const {
    parameter_bundle().apply(equation);
    apply_custom(equation);
    equation.f.set_constant(0.0);
}
void PDEPreset_Wave::for_each_parameter(const ForEachParameter& callback) const {
    parameter_bundle().for_each(callback);
}
double PDEPreset_Wave::evaluate_initial_condition(double, double) const {
    return init_u_param_ ? init_u_param_->value() : 0.0;
}
double PDEPreset_Wave::evaluate_initial_velocity(double, double) const {
    return init_v_param_ ? init_v_param_->value() : 0.0;
}

} // namespace fem
