#ifndef FEM_DIFFERENTIAL_EQUATION_H
#define FEM_DIFFERENTIAL_EQUATION_H

#include "core/variant.h"
#include "log_categories.h"
#include <functional>
#include <variant>
#include <type_traits>
#include <utility>

namespace fem {

template<typename Real = double>
class Coefficient {
    using ConstType  = Real;
    using LambdaType = std::function<Real(Real, Real)>;
    Variant<ConstType, LambdaType> data_;

public:
    Coefficient() : data_(Real(0)) {}
    Coefficient(Real c) : data_(c) {}

    template<typename F, typename = std::enable_if_t<!std::is_convertible_v<F, Real>>>
    Coefficient(F&& f) : data_(LambdaType(std::forward<F>(f))) {}

    Coefficient& operator=(Real v) { data_ = v; return *this; }

    template<typename F>
    std::enable_if_t<!std::is_convertible_v<F, Real>, Coefficient&>
    operator=(F&& f) { data_ = LambdaType(std::forward<F>(f)); return *this; }

    Real operator()(Real x, Real y) const {
        return data_.visit(
            [](ConstType val) -> Real { 
                return val; 
            },
            [&](auto const& func) -> Real { 
                return func(x, y); 
            }
        );
    }

    bool is_constant() const noexcept { return data_.template is<ConstType>(); }
    Real value() const {
        std::optional<ConstType> val = data_.template try_get<ConstType>(); 
        if (val == std::nullopt) {
            LOGT_ERROR(LogMath, "Coefficient uses lambda!");
            return 0.0;
        }
        return *val;
    }

    void set_constant(Real v) { data_ = v; }
    template<typename F> void set_function(F&& f) { data_ = LambdaType(std::forward<F>(f)); }
};

#define FEM_FOREACH_COEFF(F, Real) \
    F(a,        Real(1))           \
    F(c,        Real(0))           \
    F(f,        Real(0))

#define FEM_DECLARE_COEFF(name, default_value) \
    Coefficient<Real> name{ default_value };

template<typename Real = double>
struct DifferentialEquationT {
    FEM_FOREACH_COEFF(FEM_DECLARE_COEFF, Real)

    Real time{ Real(0) };
};

#undef FEM_DECLARE_COEFF
#undef FEM_FOREACH_COEFF

using DifferentialEquation = DifferentialEquationT<double>;

} // namespace fem

#endif
