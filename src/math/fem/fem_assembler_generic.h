#ifndef FEM_ASSEMBER_GENERIC
#define FEM_ASSEMBER_GENERIC

#include "fem_problem.h"
#include "fem_simulation.h"
#include "dirichlet_map.h"
#include "math/fem/fem_boundary_adapter.h"
#include "math/operators/boundary_load_model.h"
#include <algorithm>
#include <vector>

namespace fem {

template<typename IntegratorPolicy, typename Real = double>
FEMSystemT<Real> assemble_generic(const FEMProblem& P) {
    FEMSystemT<Real> sys;
    if (!P.mesh) return sys;

    const FEMMesh& M = *P.mesh;
    const Index N = M.dof_count_index();

    IntegratorPolicy integ(P);

    std::vector<Triplet> T;
    std::vector<Real> b(to_size(N), Real(0));
    T.reserve(to_size(N) * 7u + to_size(N));

    const BoundaryModel boundary = P.boundary.empty() ? make_boundary_model(M) : P.boundary;
    const DirichletMask D = build_dirichlet_mask(boundary, M.dof_count_count());

    Real Ke[3][3];
    Real be[3];

    for (const auto& E : M.elems) {
        integ.element(M, E, Ke, be);

        for (int li = 0; li < 3; ++li) {
            const Index I = E.v[li];
            if (!is_valid(I, b.size())) continue;

            // RHS volume. Dirichlet rows are prescribed below.
            if (!D.contains(I)) {
                b[to_size(I)] += be[li];
            }

            for (int lj = 0; lj < 3; ++lj) {
                const Index J = E.v[lj];
                if (!is_valid(J, b.size())) continue;

                const Real aIJ = Ke[li][lj];

                if (!D.contains(I) && !D.contains(J)) {
                    T.push_back({I, J, static_cast<Real>(aIJ)});
                } else if (!D.contains(I) && D.contains(J)) {
                    b[to_size(I)] -= aIJ * static_cast<Real>(D.value[to_size(J)]);
                }
            }
        }
    }

    BoundaryLoadModel{}.add_natural_terms(boundary, M, T, b);

    if (!T.empty()) {
        std::vector<Triplet> filtered;
        filtered.reserve(T.size() + D.is_dirichlet.size());
        for (const Triplet& tr : T) {
            if (!is_valid(tr.r, D.is_dirichlet.size()) || !is_valid(tr.c, D.is_dirichlet.size())) continue;
            if (D.contains(tr.r) || D.contains(tr.c)) continue;
            filtered.push_back(tr);
        }
        T.swap(filtered);
    }

    for (Index i = 0; i < N; ++i) {
        if (!D.contains(i)) continue;
        T.push_back({i, i, Real(1)});
        b[to_size(i)] = static_cast<Real>(D.value[to_size(i)]);
    }

    sys.A = build_crs_from_triplets(N, std::move(T));
    sys.b = std::move(b);
    sys.x.assign(to_size(N), Real(0));

    for (Index i = 0; i < N; ++i) {
        if (D.contains(i)) {
            sys.x[to_size(i)] = static_cast<Real>(D.value[to_size(i)]);
        }
    }

    return sys;
}

} // namespace fem

#endif
