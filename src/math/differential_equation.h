#ifndef FEM_DIFFERENTIAL_EQUATION_H
#define FEM_DIFFERENTIAL_EQUATION_H

#include <functional>
#include <variant>
#include <type_traits>
#include <utility>

namespace fem {

template<typename Real = double>
class Coefficient {
    using ConstType  = Real;
    using LambdaType = std::function<Real(Real, Real)>;
    std::variant<ConstType, LambdaType> data_;

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
        return std::visit([&](auto&& arg) -> Real {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ConstType>) return arg;
            else return arg(x, y);
        }, data_);
    }

    bool is_constant() const noexcept { return std::holds_alternative<ConstType>(data_); }
    Real value() const { return std::get<ConstType>(data_); }

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
