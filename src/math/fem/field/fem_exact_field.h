#pragma once
#include "math/differential_equation.h"
#include "fem_field_view.h"

namespace fem {

template<typename Real>
struct ExactFieldCtx {
    const Coefficient<Real>* u  = nullptr;
    const Coefficient<Real>* ux = nullptr;
    const Coefficient<Real>* uy = nullptr;
};

template<typename Real>
static Real exact_eval_u(const void* c, Real x, Real y) noexcept {
    const auto* ctx = (const ExactFieldCtx<Real>*)c;
    return ctx->u ? (*ctx->u)(x,y) : Real(0);
}

template<typename Real>
static void exact_eval_grad(const void* c, Real x, Real y, Real& gx, Real& gy) noexcept {
    const auto* ctx = (const ExactFieldCtx<Real>*)c;
    gx = ctx->ux ? (*ctx->ux)(x,y) : Real(0);
    gy = ctx->uy ? (*ctx->uy)(x,y) : Real(0);
}

} // namespace fem
