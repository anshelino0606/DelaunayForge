#ifndef FEM_PDE_PDE_MODEL_H
#define FEM_PDE_PDE_MODEL_H

#include "math/differential_equation.h"

namespace fem {

template<typename Real = double>
struct PDEModelT {
    Coefficient<Real> a{Real(1)};
    Coefficient<Real> c{Real(0)};
    Coefficient<Real> f{Real(0)};
    Real time{Real(0)};

    PDEModelT() = default;

    explicit PDEModelT(const DifferentialEquationT<Real>& equation)
        : a(equation.a), c(equation.c), f(equation.f), time(equation.time) {}

    void apply_to(DifferentialEquationT<Real>& equation) const {
        equation.a = a;
        equation.c = c;
        equation.f = f;
        equation.time = time;
    }
};

using PDEModel = PDEModelT<double>;

} // namespace fem

#endif // FEM_PDE_PDE_MODEL_H
