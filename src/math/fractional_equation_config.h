#ifndef FRACTIONAL_EQUATION_CONFIG
#define FRACTIONAL_EQUATION_CONFIG

#include "core/object/object.h"
#include <cstdint>

namespace fem {

FEM_DECLARE_ENUM(
    FractionalType,
    Integral,    // restricted / Riesz (killed outside, exterior Dirichlet)
    Spectral,    // fractional power of local operator with BCs
    Regional     // censored/regional operator (integration over Ω only)
);

/// Configuration for fractional differential equations
template<typename Real>
struct alignas(32) FractionalEquationConfigT {
    Real s = Real(0.5);            ///< Fractional order: s ∈ (0, 1)
    Real scale = Real(1.0);        ///< Scaling factor for fractional operator  
    Real eig_clip = Real(0);       ///< Eigenvalue clipping threshold
    
    FractionalType type = FractionalType::Spectral;  ///< Operator type selector
    int spectral_k = -1;                             ///< Spectral truncation (-1 = all modes)
    
    constexpr auto operator<=>(const FractionalEquationConfigT&) const = default;
};

using FractionalEquationConfig = FractionalEquationConfigT<double>;

using FractionalEquationConfigF = FractionalEquationConfigT<float>;

} // namespace fem

#endif
