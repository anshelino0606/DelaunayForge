// fem_assembler.cpp
#include "fem_assembler.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <ranges>
#include "fem_simulation.h"
#include "fem_problem.h"
#include "math/differential_equation_solution.h"
#include "fem_fractional_spectral_p1.h"
#include "fem_assembler_generic.h"
#include "fem_integrators.h"

namespace fem {

static void add_entry(std::vector<Triplet>& T, int i, int j, double v){
    T.push_back({i,j,v});
}

CRS build_crs_from_triplets(int n, std::vector<Triplet> T) {
    std::sort(T.begin(), T.end(), [](const Triplet& a, const Triplet& b){
        return std::tie(a.r, a.c) < std::tie(b.r, b.c);
    });

    std::vector<int> row_ptr(n + 1, 0);
    std::vector<int> col_idx;
    std::vector<double> vals;
    col_idx.reserve(T.size());
    vals.reserve(T.size());

    int cur_row = 0;
    int last_r = -1, last_c = -1;

    for (const auto& t : T) {
        while (cur_row < t.r) {
            row_ptr[++cur_row] = (int)col_idx.size();
            last_r = -1; last_c = -1;
        }
        if (t.r == last_r && t.c == last_c) vals.back() += t.v;
        else {
            col_idx.push_back(t.c);
            vals.push_back(t.v);
            last_r = t.r; last_c = t.c;
        }
    }
    while (cur_row < n) row_ptr[++cur_row] = (int)col_idx.size();
    return CRS{ std::move(row_ptr), std::move(col_idx), std::move(vals) };
}


FEMSystem assemble_poisson_P1(const FEMProblem& P) {
    return assemble_generic<LocalIntegratorP1<double>, double>(P);
}

FEMSystem assemble_heat_implicit_euler_P1(const FEMProblem& P) {
    return assemble_generic<HeatImplicitEulerIntegratorP1<double>, double>(P);
}

FEMSystem assemble_wave_newmark_P1(const FEMProblem& P) {
    return assemble_generic<WaveNewmarkIntegratorP1<double>, double>(P);
}



void apply_dirichlet_elimination(FEMSystem& S, const FEMMesh& M,
                                 const std::vector<std::tuple<int,double>>& D)
{
    const int N = static_cast<int>(S.b.size());

    for (const auto& [i, val] : D) {
        if (i < 0 || i >= N) [[unlikely]] continue;
        
        for (int r = 0; r < N; ++r) {
            if (r == i) [[likely]] continue;

            for (int k = S.A.row_ptr[r]; k < S.A.row_ptr[r + 1]; ++k) {
                if (S.A.col_idx[k] == i) [[unlikely]] {
                    // b_r := b_r - A_ri * u_i^D
                    S.b[r] -= S.A.vals[k] * val;
                    S.A.vals[k] = 0.0; // zero column entry
                }
            }
        }

        // Zero the entire row i
        for (int k = S.A.row_ptr[i]; k < S.A.row_ptr[i + 1]; ++k) {
            S.A.vals[k] = 0.0;
        }

        // Put 1 on the diagonal A[i,i]
        bool diag_found = false;
        for (int k = S.A.row_ptr[i]; k < S.A.row_ptr[i + 1]; ++k) {
            if (S.A.col_idx[k] == i) [[unlikely]] {
                S.A.vals[k] = 1.0;
                diag_found = true;
                break;
            }
        }

        if (!diag_found) [[unlikely]] {
            int insert_at = S.A.row_ptr[i + 1];

            S.A.col_idx.insert(S.A.col_idx.begin() + insert_at, i);
            S.A.vals.insert(S.A.vals.begin() + insert_at, 1.0);

            // shift row_ptr for subsequent rows
            for (int r = i + 1; r < static_cast<int>(S.A.row_ptr.size()); ++r) {
                S.A.row_ptr[r] += 1;
            }
        }

        // Enforce the Dirichlet value on node i
        S.b[i] = val;
    }
}

FEMSystem assemble_fractional_laplacian_P1(const FEMProblem& P,
                                           double s,
                                           double C_scale = 1.0)
{
    const FEMMesh& M = *P.mesh;
    const int N = M.dof_count();

    // 1) nodal patch "masses" m_i = sum_E area(E)/3
    std::vector<double> m(N, 0.0);
    for (const auto& E : M.elems) {
        const double share = E.area / 3.0;
        m[E.v[0]] += share;
        m[E.v[1]] += share;
        m[E.v[2]] += share;
    }

    // Step 2: Assemble RHS: b_i ≈ f(x_i,y_i) * m_i
    std::vector<double> b(N);
    for (int i = 0; i < N; ++i) {
        const auto& Pn = M.nodes[i];
        b[i] = P.f(Pn.x, Pn.y) * m[i];
    }

    // 3) Dense fractional stiffness in triplet form (O(N^2))
    std::vector<Triplet> T;
    T.reserve(4 * N * N);
    const double s_exp = 1.0 + s;

    for (int i = 0; i < N; ++i) {
        const auto& Pi = M.nodes[i];
        for (int j = i + 1; j < N; ++j) {
            const auto& Pj = M.nodes[j];

            const double dx = Pi.x - Pj.x;
            const double dy = Pi.y - Pj.y;
            const double r2 = dx * dx + dy * dy;

            if (r2 == 0.0) [[unlikely]] continue;  // Skip coincident nodes

            const double denom = std::pow(r2, s_exp);           // |x_i-x_j|^{2+2s}
            const double w = C_scale * m[i] * m[j] / denom;

            // Off-diagonal entries (negative coupling)
            add_entry(T, i, j, -w);
            add_entry(T, j, i, -w);

            // Diagonal compensation: ∑_j A_ij = 0 (zero row sum)
            add_entry(T, i, i, w);
            add_entry(T, j, j, w);
        }
    }

    // 3b) Robin and Neumann boundary terms — same as classical integrators
    for (const auto& e : M.edges_bc) {
        if (e.type == BCType::None || e.type == BCType::Dirichlet) continue;

        const auto& A = M.nodes[e.a];
        const auto& B = M.nodes[e.b];
        const double L = std::hypot(B.x - A.x, B.y - A.y);
        if (L <= 0.0) continue;

        if (e.type == BCType::Robin) {
            const double k = e.k;
            const double g = e.g;

            const double m00 = L * (2.0 / 6.0);
            const double m01 = L * (1.0 / 6.0);
            const double m11 = L * (2.0 / 6.0);

            // ∫_Γ k φ_i φ_j ds  → stiffness
            add_entry(T, e.a, e.a, k * m00);
            add_entry(T, e.a, e.b, k * m01);
            add_entry(T, e.b, e.a, k * m01);
            add_entry(T, e.b, e.b, k * m11);

            // ∫_Γ g φ_i ds  → RHS
            b[e.a] += g * L * 0.5;
            b[e.b] += g * L * 0.5;
        } else if (e.type == BCType::Neumann) {
            const double gN = e.gN;
            b[e.a] += gN * L * 0.5;
            b[e.b] += gN * L * 0.5;
        }
    }

    // Step 4: Compress triplets → CRS format
    std::sort(T.begin(), T.end(), [](const Triplet& a, const Triplet& b) {
        return (a.r != b.r) ? (a.r < b.r) : (a.c < b.c);
    });

    std::vector<int> row_ptr(N + 1, 0);
    std::vector<int> col_idx;
    std::vector<double> vals;
    col_idx.reserve(T.size());
    vals.reserve(T.size());

    int cur_i = 0;
    row_ptr[0] = 0;

    for (size_t k = 0; k < T.size();) {
        int i = T[k].r;
        while (cur_i < i) {
            row_ptr[++cur_i] = static_cast<int>(col_idx.size());
        }
        
        int j = T[k].c;
        double v = 0.0;
        while (k < T.size() && T[k].r == i && T[k].c == j) {
            v += T[k].v;
            ++k;
        }
        col_idx.push_back(j);
        vals.push_back(v);
    }
    while (cur_i < N) {
        row_ptr[++cur_i] = static_cast<int>(col_idx.size());
    }

    FEMSystem S;
    S.A = {std::move(row_ptr), std::move(col_idx), std::move(vals)};
    S.b = std::move(b);
    S.x.assign(N, 0.0);

    return S;
}

FEMSystem assemble_and_solve_spectral_fractional_P1(const FEMProblem& P, DifferentialEquationSolution& out) {
    return assemble_and_solve_fractional_spectral_P1(P, out);
}

void solve_linear_system(FEMSystem& sys) {
    FEMSolverCG cg;
    cg.tol    = kDefaultSolveTol;
    cg.max_it = kDefaultSolveMaxIt;
    cg.solve(sys.A, sys.b, sys.x);
}

}
