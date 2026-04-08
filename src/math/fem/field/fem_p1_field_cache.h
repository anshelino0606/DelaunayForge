#pragma once
#include "math/fem/fem_error_analysis.h"
#include <vector>

namespace fem {

template<typename Real>
struct P1FieldCache {
    const FEMMesh* mesh = nullptr;
    const std::vector<Real>* uh = nullptr;
    TriLocatorT<Real> locator;

    std::vector<Real> gux; // size = nt
    std::vector<Real> guy; // size = nt

    void build(const FEMMesh& M, const std::vector<Real>& U) {
        mesh = &M; uh = &U;
        locator.build(M);

        const int nt = (int)M.elems.size();
        gux.assign((size_t)nt, Real(0));
        guy.assign((size_t)nt, Real(0));

        for (int ti=0; ti<nt; ++ti) {
            const auto& E = M.elems[(size_t)ti];
            const auto& P0 = M.nodes[(size_t)E.v[0]];
            const auto& P1 = M.nodes[(size_t)E.v[1]];
            const auto& P2 = M.nodes[(size_t)E.v[2]];

            Real grad_phi[3][2];
            compute_p1_gradients<Real>((Real)P0.x,(Real)P0.y,(Real)P1.x,(Real)P1.y,(Real)P2.x,(Real)P2.y, grad_phi);

            const Real u0 = U[(size_t)E.v[0]];
            const Real u1 = U[(size_t)E.v[1]];
            const Real u2 = U[(size_t)E.v[2]];

            gux[(size_t)ti] = u0*grad_phi[0][0] + u1*grad_phi[1][0] + u2*grad_phi[2][0];
            guy[(size_t)ti] = u0*grad_phi[0][1] + u1*grad_phi[1][1] + u2*grad_phi[2][1];
        }
    }
};

template<typename Real>
static Real p1_cache_eval_u(const void* c, Real x, Real y) noexcept {
    const auto* C = (const P1FieldCache<Real>*)c;
    if (!C->mesh || !C->uh) return Real(0);

    Real l0,l1,l2;
    const int ti = C->locator.find_triangle(x,y,&l0,&l1,&l2);
    if (ti < 0) return Real(0);

    const auto& E = C->mesh->elems[(size_t)ti];
    const Real u0 = (*C->uh)[(size_t)E.v[0]];
    const Real u1 = (*C->uh)[(size_t)E.v[1]];
    const Real u2 = (*C->uh)[(size_t)E.v[2]];
    return l0*u0 + l1*u1 + l2*u2;
}

template<typename Real>
static void p1_cache_eval_grad(const void* c, Real x, Real y, Real& gx, Real& gy) noexcept {
    (void)x; (void)y;
    const auto* C = (const P1FieldCache<Real>*)c;
    if (!C->mesh) { gx=gy=Real(0); return; }

    Real l0,l1,l2;
    const int ti = C->locator.find_triangle(x,y,&l0,&l1,&l2);
    if (ti < 0) { gx=gy=Real(0); return; }

    gx = C->gux[(size_t)ti];
    gy = C->guy[(size_t)ti];
}

} // namespace fem
