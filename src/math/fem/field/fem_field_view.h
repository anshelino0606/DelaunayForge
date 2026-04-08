#pragma once
#include <cstddef>
#include "core/macro.h"


namespace fem {

template<typename Real>
struct ScalarFieldView {
    using EvalFn = Real(*)(const void* ctx, Real x, Real y) noexcept;
    using GradFn = void(*)(const void* ctx, Real x, Real y, Real& gx, Real& gy) noexcept;

    const void* ctx = nullptr;
    EvalFn eval = nullptr;
    GradFn grad = nullptr;
};

template<typename Real>
FEM_FORCEINLINE Real field_eval(const ScalarFieldView<Real>& f, Real x, Real y) noexcept {
    return f.eval ? f.eval(f.ctx, x, y) : Real(0);
}

} // namespace fem
