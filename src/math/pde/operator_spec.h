#ifndef FEM_PDE_OPERATOR_SPEC_H
#define FEM_PDE_OPERATOR_SPEC_H

#include "math/fractional_equation_config.h"
#include "math/types.h"
#include <optional>
#include <type_traits>
#include <variant>

namespace fem {

template<typename Real = double>
struct LocalEllipticSpecT {
    constexpr auto operator<=>(const LocalEllipticSpecT&) const = default;
};

template<typename Real = double>
struct FractionalIntegralSpecT {
    Real s = Real(0.5);
    Real scale = Real(1.0);
    constexpr auto operator<=>(const FractionalIntegralSpecT&) const = default;
};

template<typename Real = double>
struct FractionalRegionalSpecT {
    Real s = Real(0.5);
    Real scale = Real(1.0);
    constexpr auto operator<=>(const FractionalRegionalSpecT&) const = default;
};

template<typename Real = double>
struct FractionalSpectralSpecT {
    Real s = Real(0.5);
    Real scale = Real(1.0);
    Real eig_clip = Real(0);
    Count spectral_k = Count{0}; // 0 means automatic/all available
    constexpr auto operator<=>(const FractionalSpectralSpecT&) const = default;
};

template<typename Real = double>
using OperatorSpecT = std::variant<
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

[[nodiscard]] inline OperatorSpec make_operator_spec(const FractionalEquationConfig& cfg) {
    switch (cfg.type.value) {
        case FractionalType::Spectral:
            return FractionalSpectralSpec{cfg.s, cfg.scale, cfg.eig_clip, cfg.spectral_k > 0 ? to_count(cfg.spectral_k) : Count{0}};
        case FractionalType::Regional:
            return FractionalRegionalSpec{cfg.s, cfg.scale};
        case FractionalType::Integral:
        default:
            return FractionalIntegralSpec{cfg.s, cfg.scale};
    }
}

[[nodiscard]] inline OperatorSpec make_operator_spec(const std::optional<FractionalEquationConfig>& cfg) {
    if (!cfg) return LocalEllipticSpec{};
    return make_operator_spec(*cfg);
}

[[nodiscard]] inline std::optional<FractionalEquationConfig> make_fractional_config(const OperatorSpec& spec) {
    return std::visit([](const auto& op) -> std::optional<FractionalEquationConfig> {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, LocalEllipticSpec>) {
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, FractionalIntegralSpec>) {
            return FractionalEquationConfig{.s = op.s, .scale = op.scale, .eig_clip = 0.0, .type = FractionalType::Integral, .spectral_k = -1};
        } else if constexpr (std::is_same_v<T, FractionalRegionalSpec>) {
            return FractionalEquationConfig{.s = op.s, .scale = op.scale, .eig_clip = 0.0, .type = FractionalType::Regional, .spectral_k = -1};
        } else if constexpr (std::is_same_v<T, FractionalSpectralSpec>) {
            return FractionalEquationConfig{.s = op.s, .scale = op.scale, .eig_clip = op.eig_clip, .type = FractionalType::Spectral, .spectral_k = op.spectral_k > 0 ? static_cast<int>(op.spectral_k) : -1};
        }
    }, spec);
}

[[nodiscard]] inline bool is_local_operator(const OperatorSpec& spec) noexcept {
    return std::holds_alternative<LocalEllipticSpec>(spec);
}

[[nodiscard]] inline bool is_fractional_operator(const OperatorSpec& spec) noexcept {
    return !is_local_operator(spec);
}

[[nodiscard]] inline std::optional<FractionalType> fractional_type_of(const OperatorSpec& spec) {
    return std::visit([](const auto& op) -> std::optional<FractionalType> {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, LocalEllipticSpec>) return std::nullopt;
        else if constexpr (std::is_same_v<T, FractionalSpectralSpec>) return FractionalType::Spectral;
        else if constexpr (std::is_same_v<T, FractionalRegionalSpec>) return FractionalType::Regional;
        else return FractionalType::Integral;
    }, spec);
}

} // namespace fem

#endif // FEM_PDE_OPERATOR_SPEC_H
