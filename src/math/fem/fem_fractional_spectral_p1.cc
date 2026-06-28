#include "fem_fractional_spectral_p1.h"

#include "fem_problem.h"
#include "fem_mesh.h"
#include "fem_assembler.h"
#include "fem_boundary_adapter.h"
#include "fem_quadrature.h"
#include "math/fractional_equation_config.h"
#include "math/differential_equation_solution.h"
#include "fem_dense_linalg.h"
#include "log_categories.h"
#include "geom/geom2d/vec.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

namespace fem {


static void compute_p1_gradients(
    const FEMMesh::Node& P0,
    const FEMMesh::Node& P1,
    const FEMMesh::Node& P2,
    double grad[3][2]
) {
    const double dx1 = P1.x - P0.x, dy1 = P1.y - P0.y;
    const double dx2 = P2.x - P0.x, dy2 = P2.y - P0.y;
    const double det = dx1*dy2 - dx2*dy1; // 2*Area with sign
    const double inv_det = 1.0 / det;

    grad[0][0] = (P1.y - P2.y) * inv_det;
    grad[0][1] = (P2.x - P1.x) * inv_det;

    grad[1][0] = (P2.y - P0.y) * inv_det;
    grad[1][1] = (P0.x - P2.x) * inv_det;

    grad[2][0] = (P0.y - P1.y) * inv_det;
    grad[2][1] = (P1.x - P0.x) * inv_det;
}

struct DofSplit {
    std::vector<Index> free;
    std::vector<Index> dir;
    std::vector<int> g2l;
};

static DofSplit split_dofs_dirichlet(const FEMMesh& M, const DirichletMask& D) {
    const Index N = M.dof_count<Index>();
    DofSplit s;
    s.g2l.assign(to_size(N), -1);
    s.free.reserve(to_size(N));
    s.dir.reserve(to_size(N));

    for (Index i = 0; i < N; ++i) {
        if (D.contains(i)) [[unlikely]] {
            s.dir.push_back(i);
        } else {
            s.g2l[to_size(i)] = static_cast<int>(s.free.size());
            s.free.push_back(i);
        }
    }
    return s;
}

static void assemble_local_dense_P1(
    const FEMProblem& P,
    DenseMat& K,
    DenseMat& Mmat,
    DenseMat& Cmat,
    std::vector<double>& b,
    bool& c_is_zero
) {
    const FEMMesh& mesh = *P.mesh;
    const size_t N = mesh.dof_count();

    K = DenseMat((int)N);
    Mmat = DenseMat((int)N);

    c_is_zero = (P.c.is_constant() && P.c.value() == 0.0);
    if (!c_is_zero) {
        Cmat = DenseMat((int)N);
    } else {
        Cmat.release();
    }

    b.assign(N, 0.0);

    for (const FEMMesh::Elem& E : mesh.elems) {
        const FEMMesh::Node& P0 = mesh.nodes[E.v[0]];
        const FEMMesh::Node& P1 = mesh.nodes[E.v[1]];
        const FEMMesh::Node& P2 = mesh.nodes[E.v[2]];

        double grad[3][2];
        compute_p1_gradients(P0, P1, P2, grad);

        for (int q = 0; q < TriQuad3::n; ++q) {
            double x, y;
            tri_point(mesh, E, TriQuad3::l1[q], TriQuad3::l2[q], TriQuad3::l3[q], x, y);

            const double aq = P.a(x, y);
            const double fq = P.f(x, y);

            const double Nq[3] = {TriQuad3::l1[q], TriQuad3::l2[q], TriQuad3::l3[q]};
            const double dV = TriQuad3::w[q] * E.area;

            for (size_t i = 0; i < 3; ++i) {
                const size_t I = static_cast<size_t>(E.v[i]);
                b[I] += fq * Nq[i] * dV;

                for (size_t j = 0; j < 3; ++j) {
                    const size_t J = static_cast<size_t>(E.v[j]);
                    const double gdot = grad[i][0] * grad[j][0] + grad[i][1] * grad[j][1];

                    // Mass: ∫ φ_i φ_j
                    Mmat(I, J) += Nq[i] * Nq[j] * dV;
                    if (!c_is_zero) {
                        const double cq = P.c(x, y);
                        Cmat(I, J) += cq * Nq[i] * Nq[j] * dV;
                    }
                    // Stiffness: ∫ a ∇φ_i·∇φ_j
                    K(I, J) += aq * gdot * dV;
                }
            }
        }
    }

    // Boundary integrals: Robin and Neumann
    for (const FEMMesh::EdgeBC& e : mesh.edges_bc) {
        if (e.type == BCType::None || e.type == BCType::Dirichlet) continue;
        if (!is_valid(e.a, mesh.nodes.size()) || !is_valid(e.b, mesh.nodes.size())) continue;

        const FEMMesh::Node& A = mesh.nodes[to_size(e.a)];
        const FEMMesh::Node& B = mesh.nodes[to_size(e.b)];
        const double L = geom2d::vec::dist(B, A);

        if (e.type == BCType::Robin) {
            const double k = e.k;
            const double g = e.g;

            constexpr double m00_coeff = 2.0 / 6.0;
            constexpr double m01_coeff = 1.0 / 6.0;
            const double m00 = L * m00_coeff;
            const double m01 = L * m01_coeff;
            const double m11 = L * m00_coeff;

            // ∫_Γ k φ_i φ_j ds  → stiffness (operator part of Robin)
            K(static_cast<size_t>(e.a), static_cast<size_t>(e.a)) += k * m00;
            K(static_cast<size_t>(e.a), static_cast<size_t>(e.b)) += k * m01;
            K(static_cast<size_t>(e.b), static_cast<size_t>(e.a)) += k * m01;
            K(static_cast<size_t>(e.b), static_cast<size_t>(e.b)) += k * m11;

            // ∫_Γ g φ_i ds  → RHS (inhomogeneous Robin)
            b[to_size(e.a)] += g * L * 0.5;
            b[to_size(e.b)] += g * L * 0.5;
        } else if (e.type == BCType::Neumann) {
            // ∫_Γ gN φ_i ds  → RHS
            const double gN = e.gN;
            b[to_size(e.a)] += gN * L * 0.5;
            b[to_size(e.b)] += gN * L * 0.5;
        }
    }
}

// Extract submatrix A_ff and vector v_f
static DenseMat extract_ff(const DenseMat& A, const DofSplit& s) {
    const size_t n = s.free.size();
    DenseMat Af((int)n);
    for (size_t ii = 0; ii < n; ++ii) {
        const Index I = s.free[ii];
        for (size_t jj = 0; jj < n; ++jj) {
            const Index J = s.free[jj];
            Af((int)ii, (int)jj) = A((int)I, (int)J);
        }
    }
    return Af;
}

static std::vector<double> extract_fd_rect(
    const DenseMat& A, const DofSplit& s
) {
    const size_t n  = s.free.size();
    const size_t nd = s.dir.size();
    std::vector<double> Afd(n * nd, 0.0);
    for (size_t ii = 0; ii < n; ++ii) {
        const size_t I = static_cast<size_t>(s.free[ii]);
        const size_t row = ii * nd;
        for (size_t jd = 0; jd < nd; ++jd) {
            const size_t J = static_cast<size_t>(s.dir[jd]);
            Afd[row + jd] = A(I, J);
        }
    }
    return Afd;
}

static std::vector<double> extract_fvec(const std::vector<double>& v, const DofSplit& s) {
    const size_t n = s.free.size();
    std::vector<double> vf(n, 0.0);
    for (size_t ii = 0; ii < n; ++ii) {
        vf[ii] = v[to_size(s.free[ii])];
    }
    return vf;
}

static std::vector<double> compute_dirichlet_lifting(
    const DenseMat& K_ff,
    const std::vector<double>& K_fd,
    const std::vector<double>& g_d,
    size_t nd
) {
    const size_t n = static_cast<size_t>(K_ff.n);
    std::vector<double> rhs(n, 0.0);
    
    for (size_t i = 0; i < n; ++i) {
        double sum = 0.0;
        const size_t row = i * nd;
        for (size_t jd = 0; jd < nd; ++jd) {
            sum += K_fd[row + jd] * g_d[jd];
        }
        rhs[i] = -sum;
    }
    
    std::vector<double> g_f;
    if (!solve_spd_cholesky(K_ff, rhs, g_f)) [[unlikely]] {
        g_f.assign(n, 0.0);
    }
    return g_f;
}

// Spectral fractional Laplacian solve: (-Δ)^s u = f with homogeneous Dirichlet
FEMSystem assemble_and_solve_fractional_spectral_P1(
    const FEMProblem& P,
    DifferentialEquationSolution& out
) {
    out.invalidate();
    FEMSystem sys;

    if (!P.mesh) [[unlikely]] return sys;
    const auto* cfg = std::get_if<FractionalSpectralSpec>(&P.operator_spec());
    if (!cfg) [[unlikely]] return sys;

    const FEMMesh& mesh = *P.mesh;
    const size_t N = mesh.dof_count();

    // Assemble local matrices and RHS
    DenseMat K, Mmat, Cmat;
    std::vector<double> b;
    bool c_is_zero = false;
    assemble_local_dense_P1(P, K, Mmat, Cmat, b, c_is_zero);

    // Split DOFs into free and Dirichlet
    const DirichletMask D = build_dirichlet_mask(mesh);
    DofSplit s = split_dofs_dirichlet(mesh, D);

    const size_t n = s.free.size();
    const size_t nd = s.dir.size();

    sys.x.assign(N, 0.0);
    sys.b = b;

    // Extract and set Dirichlet values
    std::vector<double> g_d(nd, 0.0);
    for (size_t jd = 0; jd < nd; ++jd) {
        const Index gi = s.dir[jd];
        g_d[jd] = D.value[to_size(gi)];
        sys.x[to_size(gi)] = g_d[jd];
    }

    if (n == 0) [[unlikely]] {
        fill_solution(sys, out);
        return sys;
    }

    DenseMat K_ff = extract_ff(K, s);
    DenseMat M_ff = extract_ff(Mmat, s);
    DenseMat C_ff;
    if (!c_is_zero) C_ff = extract_ff(Cmat, s);
    std::vector<double> b_f = extract_fvec(b, s);

    // Inhomogeneous Dirichlet: extract K_fd while full K is still alive
    bool has_inhom_dir = std::ranges::any_of(g_d, [](double v) { return v != 0.0; });
    std::vector<double> g_f;
    if (has_inhom_dir) [[unlikely]] {
        std::vector<Real> K_fd = extract_fd_rect(K, s);
        g_f = compute_dirichlet_lifting(K_ff, K_fd, g_d, nd);
    }

    K.release();
    Mmat.release();
    Cmat.release();

    std::vector<double> Mg;
    if (has_inhom_dir) [[unlikely]] {
        Mg.resize(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
            double sum = 0.0;
            for (size_t j = 0; j < n; ++j)
                sum += M_ff(i, j) * g_f[j];
            Mg[i] = sum;
        }
    }

    GenSymEig ge = generalized_selfadjoint_eig_inplace(K_ff, M_ff);

    // Clip small eigenvalues
    const double eig_clip_d = cfg->eig_clip;
    std::ranges::for_each(ge.lambda, [eig_clip_d](double& lam) {
        if (lam < eig_clip_d) lam = 0.0;
    });

    // Modal truncation
    size_t use_k = n;
    if (cfg->spectral_k > 0) {
        use_k = std::min(use_k, static_cast<size_t>(cfg->spectral_k));
    }

    // Use ge.Phi directly — no copy into a separate n×n matrix (saves n²)
    const DenseMat& Phi = ge.Phi;

    // Project RHS: b_hat = Phi^T * b_f
    std::vector<double> b_hat(use_k, 0.0);
    for (size_t k = 0; k < use_k; ++k) {
        double sum = 0.0;
        for (size_t i = 0; i < n; ++i)
            sum += Phi(i, k) * b_f[i];
        b_hat[k] = sum;
    }
    { std::vector<double>().swap(b_f); }  // free b_f

    // Inhomogeneous Dirichlet lifting in modal space (uses precomputed Mg)
    if (has_inhom_dir) [[unlikely]] {
        std::vector<double> alpha_g(use_k, 0.0);
        for (size_t k = 0; k < use_k; ++k) {
            double sum = 0.0;
            for (size_t i = 0; i < n; ++i)
                sum += Phi(i, k) * Mg[i];
            alpha_g[k] = sum;
        }
        { std::vector<double>().swap(Mg); }  // free Mg

        const double sf = cfg->scale;
        for (size_t k = 0; k < use_k; ++k) {
            const double lam = ge.lambda[k];
            const double ds = (lam > 0.0) ? std::pow(lam, cfg->s) : 0.0;
            b_hat[k] -= sf * ds * alpha_g[k];
        }
    }

    const double scale_factor = cfg->scale;
    const double s_param = cfg->s;

    std::vector<double> spectral_diag(use_k);
    for (size_t k = 0; k < use_k; ++k) {
        const double lam = ge.lambda[k];
        spectral_diag[k] = (lam > 0.0)
            ? scale_factor * std::pow(lam, s_param) : 0.0;
    }

    // Project reaction: C_hat = Phi^T * C_ff * Phi
    DenseMat A_hat(use_k);
    if (!c_is_zero) {
        // CP[i * use_k + k] = Σ_j C_ff(i,j) · Phi(j,k)
        const size_t cp_sz = n * use_k;
        std::vector<double> CP(cp_sz, 0.0);
        for (size_t i = 0; i < n; ++i) {
            const size_t row = i * use_k;
            for (size_t k = 0; k < use_k; ++k) {
                double sum = 0.0;
                for (size_t j = 0; j < n; ++j)
                    sum += C_ff(i, j) * Phi(j, k);
                CP[row + k] = sum;
            }
        }
        C_ff.release();  // free C_ff

        // A_hat(p,q) = Σ_i Phi(i,p) · CP(i,q)
        for (size_t p = 0; p < use_k; ++p) {
            for (size_t q = 0; q < use_k; ++q) {
                double sum = 0.0;
                for (size_t i = 0; i < n; ++i)
                    sum += Phi(i, p) * CP[i * use_k + q];
                A_hat(p, q) = sum;
            }
        }
    }

    // Add spectral diagonal
    for (size_t k = 0; k < use_k; ++k)
        A_hat(k, k) += spectral_diag[k];

    // Check for singular Neumann/Robin problem
    if (use_k > 0 && ge.lambda[0] == 0.0) [[unlikely]] {
        const double a00 = std::abs(A_hat(0, 0));
        if (a00 < 1e-14 && std::abs(b_hat[0]) > 1e-10) [[unlikely]] {
            LOGT_INFO(LogMath,
                "[spectral frac] singular Neumann/Robin-type operator (λ0=0, no reaction). "
                "RHS not compatible (b̂0=%.6e). Consider adding reaction c>0 or enforcing compatibility.",
                b_hat[0]
            );
            b_hat[0] = 0.0;
        }
    }

    std::vector<double> u_hat;
    if (!solve_spd_cholesky(A_hat, b_hat, u_hat)) [[unlikely]] {
        LOGT_INFO(LogMath, "[spectral frac] modal solve failed (A_hat not SPD).");
        u_hat.assign(use_k, 0.0);
    }

    // Cache spectral bilinear energy: (1/2) scale · Σ λ^s · û²
    {
        double e = 0.0;
        for (size_t k = 0; k < use_k; ++k) {
            const double lam = ge.lambda[k];
            if (lam > eig_clip_d) {
                e += scale_factor * std::pow(lam, s_param) *
                     u_hat[k] * u_hat[k];
            }
        }
        out.spectral_bilinear_energy = 0.5 * e;
    }

    for (size_t li = 0; li < n; ++li) {
        double sum = 0.0;
        for (size_t k = 0; k < use_k; ++k)
            sum += Phi(li, k) * u_hat[k];
        const Index gi = s.free[li];
        sys.x[to_size(gi)] = sum + (has_inhom_dir ? g_f[li] : 0.0);
    }

    fill_solution(sys, out);
    return sys;
}


} // namespace fem
