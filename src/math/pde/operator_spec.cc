#include "operator_spec.h"

namespace fem {

namespace {

struct FractionalConfigVisitor {
    std::optional<FractionalEquationConfig> operator()(const LocalEllipticSpec&) const {
        return std::nullopt;
    }

    std::optional<FractionalEquationConfig> operator()(const FractionalIntegralSpec& op) const {
        return FractionalEquationConfig{
            .s = op.s,
            .scale = op.scale,
            .eig_clip = 0.0,
            .type = FractionalType::Integral,
            .spectral_k = -1
        };
    }

    std::optional<FractionalEquationConfig> operator()(const FractionalRegionalSpec& op) const {
        return FractionalEquationConfig{
            .s = op.s,
            .scale = op.scale,
            .eig_clip = 0.0,
            .type = FractionalType::Regional,
            .spectral_k = -1
        };
    }

    std::optional<FractionalEquationConfig> operator()(const FractionalSpectralSpec& op) const {
        return FractionalEquationConfig{
            .s = op.s,
            .scale = op.scale,
            .eig_clip = op.eig_clip,
            .type = FractionalType::Spectral,
            .spectral_k = op.spectral_k > Count{0} ? static_cast<int>(op.spectral_k) : -1
        };
    }
};

struct FractionalTypeVisitor {
    std::optional<FractionalType> operator()(const LocalEllipticSpec&) const {
        return std::nullopt;
    }

    std::optional<FractionalType> operator()(const FractionalIntegralSpec&) const {
        return FractionalType::Integral;
    }

    std::optional<FractionalType> operator()(const FractionalRegionalSpec&) const {
        return FractionalType::Regional;
    }

    std::optional<FractionalType> operator()(const FractionalSpectralSpec&) const {
        return FractionalType::Spectral;
    }
};

} // namespace

OperatorSpec make_operator_spec(const FractionalEquationConfig& cfg) {
    switch (cfg.type.value) {
        case FractionalType::Spectral:
            return FractionalSpectralSpec{
                cfg.s,
                cfg.scale,
                cfg.eig_clip,
                cfg.spectral_k > 0 ? to_count(cfg.spectral_k) : Count{0}
            };
        case FractionalType::Regional:
            return FractionalRegionalSpec{cfg.s, cfg.scale};
        case FractionalType::Integral:
        default:
            return FractionalIntegralSpec{cfg.s, cfg.scale};
    }
}

OperatorSpec make_operator_spec(const std::optional<FractionalEquationConfig>& cfg) {
    if (!cfg) return LocalEllipticSpec{};
    return make_operator_spec(*cfg);
}

std::optional<FractionalEquationConfig> make_fractional_config(const OperatorSpec& spec) {
    return spec.apply(FractionalConfigVisitor{});
}

bool is_local_operator(const OperatorSpec& spec) noexcept {
    return spec.is<LocalEllipticSpec>();
}

bool is_fractional_operator(const OperatorSpec& spec) noexcept {
    return !is_local_operator(spec);
}

std::optional<FractionalType> fractional_type_of(const OperatorSpec& spec) {
    return spec.apply(FractionalTypeVisitor{});
}

} // namespace fem
