#ifndef FEM_ASSEMBER_GENERIC
#define FEM_ASSEMBER_GENERIC

#include "fem_problem.h"
#include "fem_simulation.h"
#include "dirichlet_map.h"
#include <vector>
#include <algorithm>
#include <tuple>

namespace fem {

template<typename IntegratorPolicy, typename Real = double>
FEMSystemT<Real> assemble_generic(const FEMProblem& P) {
    FEMSystemT<Real> sys;
    if (!P.mesh) return sys;

    const FEMMesh& M = *P.mesh;
    const int N = M.dof_count();

    IntegratorPolicy integ(P);

    std::vector<Triplet> T;
    std::vector<Real> b(N, Real(0));
    T.reserve((size_t)N * 7);

    const DirichletData D = build_dirichlet_data(M);

    Real Ke[3][3];
    Real be[3];

    for (const auto& E : M.elems) {
        integ.element(M, E, Ke, be);

        for (int li = 0; li < 3; ++li) {
            const int I = E.v[li];

            // RHS volume
            if (!D.is_dirichlet[I]) {
                b[I] += be[li];
            }

            for (int lj = 0; lj < 3; ++lj) {
                const int J = E.v[lj];
                const Real aIJ = Ke[li][lj];

                if (!D.is_dirichlet[I] && !D.is_dirichlet[J]) {
                    T.push_back({I, J, (double)aIJ});
                } else if (!D.is_dirichlet[I] && D.is_dirichlet[J]) {
                    b[I] -= aIJ * (Real)D.value[J];
                }
                // if I is Dirichlet: skip row contributions; we’ll set row later.
            }
        }
    }

    for (const auto& e : M.edges_bc) {
        if (e.type == BCType::Dirichlet || e.type == BCType::None) continue;

        integ.boundary(M, e, T, b);
    }

    if (!T.empty()) {
        std::vector<Triplet> Tf;
        Tf.reserve(T.size());
        for (auto& tr : T) {
            if (D.is_dirichlet[tr.r] || D.is_dirichlet[tr.c]) continue;
            Tf.push_back(tr);
        }
        T.swap(Tf);
    }

    sys.A = build_crs_from_triplets(N, std::move(T));
    sys.b = std::move(b);
    sys.x.assign(N, Real(0));

    for (int i = 0; i < N; ++i) {
        if (!D.is_dirichlet[i]) continue;

        // zero row i
        for (int k = sys.A.row_ptr[i]; k < sys.A.row_ptr[i+1]; ++k) {
            sys.A.vals[k] = 0.0;
        }

        bool diag_found = false;
        for (int k = sys.A.row_ptr[i]; k < sys.A.row_ptr[i+1]; ++k) {
            if (sys.A.col_idx[k] == i) {
                sys.A.vals[k] = 1.0;
                diag_found = true;
                break;
            }
        }
        if (!diag_found) {
            const int insert_at = sys.A.row_ptr[i+1];
            sys.A.col_idx.insert(sys.A.col_idx.begin() + insert_at, i);
            sys.A.vals.insert(sys.A.vals.begin() + insert_at, 1.0);
            for (int r = i + 1; r < (int)sys.A.row_ptr.size(); ++r) sys.A.row_ptr[r] += 1;
        }

        sys.b[i] = (Real)D.value[i];
        sys.x[i] = (Real)D.value[i];
    }

    return sys;
}


} // namespace fem

#endif
