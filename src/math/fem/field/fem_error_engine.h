#pragma once
#include "fem_error_results.h"
#include "fem_field_view.h"
#include "math/fem/fem_mesh.h"
#include "math/fem/fem_quadrature.h"
#include "math/fem/fem_error_analysis.h"

namespace fem {

template<typename Real>
static ErrorResults compute_against_field(
    const FEMMesh& coarse,
    const std::vector<Real>& uh_coarse,
    const ScalarFieldView<Real>& ref,
    const ScalarFieldView<Real>* ref_grad,
    const CRS* A_for_energy
) {
    ErrorResults out;
    if ((int)uh_coarse.size() != coarse.dof_count() || coarse.elems.empty()) return out;

    {
        Real linf = Real(0);
        for (int i=0; i<coarse.dof_count(); ++i) {
            const auto& n = coarse.nodes[(size_t)i];
            const Real uref = field_eval<Real>(ref, (Real)n.x, (Real)n.y);
            linf = std::max(linf, (Real)std::abs(uref - uh_coarse[(size_t)i]));
        }
        out.linf_nodes = (double)linf;
    }

    Real l2_sq = Real(0);
    Real h1_sq = Real(0);
    bool has_h1 = (ref_grad && ref_grad->grad);

    for (const auto& E : coarse.elems) {
        const auto& P0 = coarse.nodes[(size_t)E.v[0]];
        const auto& P1 = coarse.nodes[(size_t)E.v[1]];
        const auto& P2 = coarse.nodes[(size_t)E.v[2]];

        const Real x0=(Real)P0.x, y0=(Real)P0.y;
        const Real x1=(Real)P1.x, y1=(Real)P1.y;
        const Real x2=(Real)P2.x, y2=(Real)P2.y;

        const Real u0 = uh_coarse[(size_t)E.v[0]];
        const Real u1 = uh_coarse[(size_t)E.v[1]];
        const Real u2 = uh_coarse[(size_t)E.v[2]];

        Real grad_phi[3][2];
        compute_p1_gradients<Real>(x0,y0,x1,y1,x2,y2, grad_phi);
        const Real gux = u0*grad_phi[0][0] + u1*grad_phi[1][0] + u2*grad_phi[2][0];
        const Real guy = u0*grad_phi[0][1] + u1*grad_phi[1][1] + u2*grad_phi[2][1];

        for (int q=0; q<TriQuad3::n; ++q) {
            const Real L0=(Real)TriQuad3::l1[q];
            const Real L1=(Real)TriQuad3::l2[q];
            const Real L2=(Real)TriQuad3::l3[q];

            const Real xq = L0*x0 + L1*x1 + L2*x2;
            const Real yq = L0*y0 + L1*y1 + L2*y2;

            const Real uhq = L0*u0 + L1*u1 + L2*u2;
            const Real uref = field_eval<Real>(ref, xq, yq);

            const Real diff = uref - uhq;
            l2_sq += (Real)(E.area * TriQuad3::w[q]) * diff*diff;

            if (has_h1) {
                Real rgx=0,rgy=0;
                ref_grad->grad(ref_grad->ctx, xq, yq, rgx, rgy);
                const Real dx = rgx - gux;
                const Real dy = rgy - guy;
                h1_sq += (Real)(E.area * TriQuad3::w[q]) * (dx*dx + dy*dy);
            }
        }
    }

    out.l2 = (double)std::sqrt(l2_sq);
    out.has_h1 = has_h1;
    out.h1_semi = has_h1 ? (double)std::sqrt(h1_sq) : 0.0;

    if (A_for_energy && (int)A_for_energy->row_ptr.size() == coarse.dof_count()+1) {
        std::vector<Real> e((size_t)coarse.dof_count(), Real(0));
        for (int i=0; i<coarse.dof_count(); ++i) {
            const auto& n = coarse.nodes[(size_t)i];
            e[(size_t)i] = field_eval<Real>(ref, (Real)n.x, (Real)n.y) - uh_coarse[(size_t)i];
        }

        Real eAe = Real(0);
        for (int i=0;i<coarse.dof_count();++i) {
            Real s = Real(0);
            for (int k=A_for_energy->row_ptr[i]; k<A_for_energy->row_ptr[i+1]; ++k) {
                const int j = A_for_energy->col_idx[k];
                s += (Real)A_for_energy->vals[k] * e[(size_t)j];
            }
            eAe += e[(size_t)i] * s;
        }
        out.has_energy = true;
        out.energy = (double)((eAe > Real(0)) ? std::sqrt(eAe) : Real(0));
    }

    out.valid = true;
    return out;
}

} // namespace fem
