#include "pde_rhs_kinds.h"
#include "log_categories.h"
#include "math/math_.h"
#include <cmath>

using namespace fem::RHSParameters;

namespace fem {

FEM_DEFINE_RHS_SCALAR_PARAMETER(
    FConstant, 
    DisplayName("f (const)"),
    DragSpeed(0.01f),
    ClampMin(-1e6f),
    ClampMax(1e6f),
    Format("%.3g")
);

FEM_DEFINE_RHS_SCALAR_PARAMETER(
    A, 
    DisplayName("A"),
    DragSpeed(0.01f),
    ClampMin(-1e3f),
    ClampMax(1e3f),
    Format("%.3g")
);

FEM_DEFINE_RHS_SCALAR_PARAMETER(
    KX, 
    DisplayName("kx"),
    DragSpeed(0.01f),
    ClampMin(-10.0f),
    ClampMax(10.0f),
    Format("%.2f")
);

FEM_DEFINE_RHS_SCALAR_PARAMETER(
    KY, 
    DisplayName("ky"),
    DragSpeed(0.01f),
    ClampMin(-10.0f),
    ClampMax(10.0f),
    Format("%.2f")
);

FEM_DEFINE_RHS(
    RHS_FConstant,
    "Constants F",
    FConstant
);

void apply(DifferentialEquation& equation, const RHS_FConstantParams& ctx) {
    const double f_val = ctx.FConstant_->value(equation.time);
    equation.f.set_constant(f_val);
}

FEM_DEFINE_RHS(
    RHS_Sin,
    "A*sin(kx*x)*sin(ky*y)",
    A,
    KX,
    KY
);

void apply(DifferentialEquation& equation, const RHS_SinParams& ctx) {
    const double A_val = ctx.A_->value(equation.time);
    const double kx_val = ctx.KX_->value(equation.time);
    const double ky_val = ctx.KY_->value(equation.time);
    
    // Set f as a lambda function: f(x,y) = A*sin(kx*x)*sin(ky*y)
    equation.f.set_function([A_val, kx_val, ky_val](double x, double y) {
        return A_val * std::sin(kx_val * x) * std::sin(ky_val * y);
    });
}

FEM_DEFINE_RHS(
    RHS_PoissonManufactured,
    "2*pi^2*A*sin(pi*x)*sin(pi*y)",
    A
);

void apply(DifferentialEquation& equation, const RHS_PoissonManufacturedParams& ctx) {
    const double A_val = ctx.A_->value(equation.time);
    const double coeff = 2.0 * Math::PI * Math::PI * A_val;
    equation.f.set_function([coeff](double x, double y) {
        return coeff * std::sin(Math::PI * x) * std::sin(Math::PI * y);
    });
}

}