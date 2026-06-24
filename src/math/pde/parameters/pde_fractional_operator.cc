#include "pde_fractional_operator.h"

namespace fem::PDEParameters {

FEM_DEFINE_OBJECT(FractionalOperator, PDEParameter, DisplayName("Fractional Operator"));

FEM_BEGIN_PROPERTY_REGISTER(FractionalOperator)
{
    FEM_REGISTER_PROPERTY(FractionalOperator, type_,
        DisplayName("Type"),
        NoTypeHeader()
    );

    FEM_REGISTER_PROPERTY(FractionalOperator, s_,
        DisplayName("s (order)"),
        DragSpeed(0.005f),
        ClampMin(0.001f),
        ClampMax(0.999f),
        Format("%.4f"),
        NoTypeHeader()
    );

    FEM_REGISTER_PROPERTY(FractionalOperator, scale_,
        DisplayName("Scale"),
        DragSpeed(0.01f),
        ClampMin(0.0f),
        ClampMax(1e12f),
        Format("%.6g"),
        NoTypeHeader()
    );

    FEM_REGISTER_PROPERTY(FractionalOperator, spectral_k_,
        DisplayName("Spectral modes (k)"),
        SHOW_FOR_ENUM(type_, fem::FractionalType::Spectral),
        NoTypeHeader()
    );

    FEM_REGISTER_PROPERTY(FractionalOperator, eig_clip_,
        DisplayName("Eigen clip"),
        SHOW_FOR_ENUM(type_, fem::FractionalType::Spectral),
        DragSpeed(1e-12f),
        ClampMin(0.0f),
        ClampMax(1e-2f),
        Format("%.3e"),
        NoTypeHeader()
    );
}
FEM_END_PROPERTY_REGISTER(FractionalOperator)

FractionalEquationConfig FractionalOperator::config() const {
    return FractionalEquationConfig{
        .s = s_,
        .scale = scale_,
        .eig_clip = eig_clip_,
        .type = type_,
        .spectral_k = spectral_k_
    };
}

OperatorSpec FractionalOperator::operator_spec() const {
    return make_operator_spec(config());
}

void FractionalOperator::apply([[maybe_unused]] DifferentialEquation& equation) const {}

} // namespace fem::PDEParameters
