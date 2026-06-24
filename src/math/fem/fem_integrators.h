#ifndef FEM_INTEGRATORS
#define FEM_INTEGRATORS

#include "fem_mesh.h"
#include "fem_problem.h"
#include "fem_assembler.h"
#include "math/differential_equation.h"
#include "fem_quadrature.h"
#include "math/pde/time_integration.h"
#include <vector>
#include <cmath>

namespace fem {

namespace detail {
    inline void compute_p1_gradients(
        const FEMMesh::Node& P0,
        const FEMMesh::Node& P1,
        const FEMMesh::Node& P2,
        double grad[3][2]) noexcept
    {
        const double dx1 = P1.x - P0.x, dy1 = P1.y - P0.y;
        const double dx2 = P2.x - P0.x, dy2 = P2.y - P0.y;
        const double det = dx1*dy2 - dx2*dy1;  // 2*Area

        if (std::abs(det) <= 1e-14) [[unlikely]] {
            grad[0][0] = grad[0][1] = 0.0;
            grad[1][0] = grad[1][1] = 0.0;
            grad[2][0] = grad[2][1] = 0.0;
            return;
        }

        const double inv_det = 1.0 / det;
        
        grad[0][0] = (P1.y - P2.y) * inv_det;
        grad[0][1] = (P2.x - P1.x) * inv_det;
        
        grad[1][0] = (P2.y - P0.y) * inv_det;
        grad[1][1] = (P0.x - P2.x) * inv_det;
        
        grad[2][0] = (P0.y - P1.y) * inv_det;
        grad[2][1] = (P1.x - P0.x) * inv_det;
    }
    
    [[nodiscard]] constexpr int safe_quad_index(int q) noexcept {
        return q;
    }
}

template<typename Real>
struct LocalIntegratorP1 {
    const FEMProblem& P;
    explicit LocalIntegratorP1(const FEMProblem& prob) : P(prob) {}

    void element(const FEMMesh& mesh, const FEMMesh::Elem& E,
                 Real (&Ke)[3][3], Real (&be)[3]) const
    {
        for (int i = 0; i < 3; ++i) {
            be[i] = 0;
            for (int j = 0; j < 3; ++j) Ke[i][j] = 0;
        }
        
        const auto& P0 = mesh.nodes[E.v[0]];
        const auto& P1 = mesh.nodes[E.v[1]];
        const auto& P2 = mesh.nodes[E.v[2]];
        
        double grad[3][2];
        detail::compute_p1_gradients(P0, P1, P2, grad);
        
        for (int q = 0; q < fem::TriQuad3::n; ++q) {
            double x, y;
            fem::tri_point(mesh, E,
                          fem::TriQuad3::l1[q], 
                          fem::TriQuad3::l2[q], 
                          fem::TriQuad3::l3[q],
                          x, y);
            
            const double aq = (double)P.a(x, y);
            const double cq = (double)P.c(x, y);
            const double fq = (double)P.f(x, y);
            const double wq = fem::TriQuad3::w[q];
            
            const double N[3] = {
                fem::TriQuad3::l1[q],
                fem::TriQuad3::l2[q],
                fem::TriQuad3::l3[q]
            };
            
            // Stiffness: ∫ a(x) ∇φᵢ·∇φⱼ dx
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    double grad_dot = grad[i][0]*grad[j][0] + 
                                     grad[i][1]*grad[j][1];
                    Ke[i][j] += (Real)(wq * aq * grad_dot * E.area);
                }
            }
            
            // Mass: ∫ c(x) φᵢ φⱼ dx
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    Ke[i][j] += (Real)(wq * cq * N[i] * N[j] * E.area);
                }
            }
            
            // RHS: ∫ f(x) φᵢ dx
            for (int i = 0; i < 3; ++i) {
                be[i] += (Real)(wq * fq * N[i] * E.area);
            }
        }
    }
    };

template<typename Real>
struct HeatImplicitEulerIntegratorP1 {
    const FEMProblem& P;
    explicit HeatImplicitEulerIntegratorP1(const FEMProblem& prob) : P(prob) {}

    void element(const FEMMesh& mesh, const FEMMesh::Elem& E,
                 Real (&Ke)[3][3], Real (&be)[3]) const
    {
        for (int i = 0; i < 3; ++i) {
            be[i] = 0;
            for (int j = 0; j < 3; ++j) Ke[i][j] = 0;
        }

        const double dt = (P.dt > 0.0) ? P.dt : 0.0;
        const bool has_prev = !P.u_prev.empty();

        const auto& P0 = mesh.nodes[E.v[0]];
        const auto& P1 = mesh.nodes[E.v[1]];
        const auto& P2 = mesh.nodes[E.v[2]];

        double grad[3][2];
        detail::compute_p1_gradients(P0, P1, P2, grad);

        for (int q = 0; q < fem::TriQuad3::n; ++q) {
            double x, y;
            fem::tri_point(mesh, E,
                          fem::TriQuad3::l1[q],
                          fem::TriQuad3::l2[q],
                          fem::TriQuad3::l3[q],
                          x, y);

            const double aq = (double)P.a(x, y);
            const double cq = (double)P.c(x, y);
            const double fq = (double)P.f(x, y);
            const double wq = fem::TriQuad3::w[q];

            const double N[3] = {
                fem::TriQuad3::l1[q],
                fem::TriQuad3::l2[q],
                fem::TriQuad3::l3[q]
            };

            // Stiffness + reaction into Ke
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    double grad_dot = grad[i][0]*grad[j][0] + grad[i][1]*grad[j][1];
                    Ke[i][j] += (Real)(wq * aq * grad_dot * E.area);
                    Ke[i][j] += (Real)(wq * cq * N[i] * N[j] * E.area);
                }
            }

            // Time-derivative mass term (only if transient fields are set)
            if (dt > 0.0) [[likely]] {
                const double inv_dt = 1.0 / dt;
                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        const double m_ij = wq * N[i] * N[j] * E.area;
                        Ke[i][j] += (Real)(inv_dt * m_ij);

                        if (has_prev) [[likely]] {
                            const Index J = E.v[j];
                            const double u_prev_j = P.u_prev[to_size(J)];
                            be[i] += (Real)(inv_dt * m_ij * u_prev_j);
                        }
                    }
                }
            }

            // Load term b(t^{n+1})
            for (int i = 0; i < 3; ++i) {
                be[i] += (Real)(wq * fq * N[i] * E.area);
            }
        }
    }
};

template<typename Real>
struct WaveNewmarkIntegratorP1 {
    const FEMProblem& P;
    explicit WaveNewmarkIntegratorP1(const FEMProblem& prob) : P(prob) {}

    void element(const FEMMesh& mesh, const FEMMesh::Elem& E,
                 Real (&Ke)[3][3], Real (&be)[3]) const
    {
        for (int i = 0; i < 3; ++i) {
            be[i] = 0;
            for (int j = 0; j < 3; ++j) Ke[i][j] = 0;
        }

        // Use compile-time validated parameters
        static constexpr double beta = default_newmark.beta;
        static constexpr double gamma = default_newmark.gamma;
        
        const double dt = (P.dt > 0.0) ? P.dt : 0.0;
        const double inv_beta_dt2 = (dt > 0.0) ? (1.0 / (beta * dt * dt)) : 0.0;
        const bool has_pred = !P.u_prev.empty();

        const auto& P0 = mesh.nodes[E.v[0]];
        const auto& P1 = mesh.nodes[E.v[1]];
        const auto& P2 = mesh.nodes[E.v[2]];

        double grad[3][2];
        detail::compute_p1_gradients(P0, P1, P2, grad);

        for (int q = 0; q < fem::TriQuad3::n; ++q) {
            double x, y;
            fem::tri_point(mesh, E,
                           fem::TriQuad3::l1[q],
                           fem::TriQuad3::l2[q],
                           fem::TriQuad3::l3[q],
                           x, y);

            const double aq = (double)P.a(x, y);
            const double cq = (double)P.c(x, y);
            const double fq = (double)P.f(x, y);
            const double wq = fem::TriQuad3::w[q];

            const double N[3] = {
                fem::TriQuad3::l1[q],
                fem::TriQuad3::l2[q],
                fem::TriQuad3::l3[q]
            };

            // Effective stiffness: K + cM + (1/(β dt^2)) M
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    const double grad_dot = grad[i][0]*grad[j][0] + grad[i][1]*grad[j][1];
                    Ke[i][j] += (Real)(wq * aq * grad_dot * E.area);

                    const double m_ij = wq * N[i] * N[j] * E.area;
                    Ke[i][j] += (Real)(wq * cq * N[i] * N[j] * E.area);
                    Ke[i][j] += (Real)(inv_beta_dt2 * m_ij);

                    if (has_pred && inv_beta_dt2 != 0.0) [[likely]] {
                        const Index J = E.v[j];
                        const double u_pred_j = P.u_prev[to_size(J)];
                        be[i] += (Real)(inv_beta_dt2 * m_ij * u_pred_j);
                    }
                }
            }

            // Load term f(t^{n+1})
            for (int i = 0; i < 3; ++i) {
                be[i] += (Real)(wq * fq * N[i] * E.area);
            }
        }
    }
};


} // namespace fem

#endif
