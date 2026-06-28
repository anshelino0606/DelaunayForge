#ifndef FEM_PDE_OPERATOR_SPEC_H
#define FEM_PDE_OPERATOR_SPEC_H

#include "math/fractional_equation_config.h"
#include "math/types.h"
#include "core/variant.h"

#include <optional>
#include <variant>

namespace fem {

template<typename Real = double>
struct LocalEllipticSpecT {
    constexpr bool operator==(const LocalEllipticSpecT&) const = default;
};

template<typename Real = double>
struct FractionalIntegralSpecT {
    Real s = Real(0.5);
    Real scale = Real(1.0);
    constexpr bool operator==(const FractionalIntegralSpecT&) const = default;
};

template<typename Real = double>
struct FractionalRegionalSpecT {
    Real s = Real(0.5);
    Real scale = Real(1.0);
    constexpr bool operator==(const FractionalRegionalSpecT&) const = default;
};

template<typename Real = double>
struct FractionalSpectralSpecT {
    Real s = Real(0.5);
    Real scale = Real(1.0);
    Real eig_clip = Real(0);
    Count spectral_k = Count{0};
    constexpr bool operator==(const FractionalSpectralSpecT&) const = default;
};

template<typename Real = double>
using OperatorSpecT = Variant<
    LocalEllipticSpecT<Real>,
    FractionalIntegralSpecT<Real>,
    FractionalRegionalSpecT<Real>,
    FractionalSpectralSpecT<Real>
>;

using LocalEllipticSpec = LocalEllipticSpecT<double>;
using FractionalIntegralSpec = FractionalIntegralSpecT<double>;
using FractionalRegionalSpec = FractionalRegionalSpecT<double>;
using FractionalSpectralSpec = FractionalSpectralSpecT<double>;
using OperatorSpec = OperatorSpecT<double>;

[[nodiscard]] OperatorSpec make_operator_spec(const FractionalEquationConfig& cfg);
[[nodiscard]] OperatorSpec make_operator_spec(const std::optional<FractionalEquationConfig>& cfg);
[[nodiscard]] std::optional<FractionalEquationConfig> make_fractional_config(const OperatorSpec& spec);
[[nodiscard]] bool is_local_operator(const OperatorSpec& spec) noexcept;
[[nodiscard]] bool is_fractional_operator(const OperatorSpec& spec) noexcept;
[[nodiscard]] std::optional<FractionalType> fractional_type_of(const OperatorSpec& spec);

} // namespace fem

#endif // FEM_PDE_OPERATOR_SPEC_H
