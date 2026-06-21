#include "fractional_integral_operator.h"
#include "fem/fem_mesh.h"
#include "fem/fem_problem.h"
#include "fractional_equation_config.h"

namespace fem {

FractionalIntegralOperator::FractionalIntegralOperator(const glm::ivec3& global_vertex_indices) 
    : vertex_indices_(global_vertex_indices) {}

void FractionalIntegralOperator::compute(
    const FEMMesh& mesh, const FractionalEquationConfig& cfg,
    const FEMProblem& prob, const std::vector<double>& nodal_mass
) {
    const double s_exp = 1.0 + static_cast<double>(cfg.s);
    const double C_scale = static_cast<double>(cfg.scale);
    const int N = mesh.dof_count();

    auto compute_weight = [&](int i, int j) -> double {
        if (i == j) return 0.0;
        const auto& ni = mesh.nodes[static_cast<size_t>(i)];
        const auto& nj = mesh.nodes[static_cast<size_t>(j)];
        
        const double dx = ni.x - nj.x;
        const double dy = ni.y - nj.y;
        const double r2 = dx*dx + dy*dy;
        if (r2 == 0.0) return 0.0;

        return C_scale * nodal_mass[static_cast<size_t>(i)] * nodal_mass[static_cast<size_t>(j)] / std::pow(r2, s_exp);
    };

    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) {
            if (a == b) continue;
            // Column major mapping: out_Af[col][row]
            Af_[b][a] = -compute_weight(vertex_indices_[a], vertex_indices_[b]);
        }
    }

    for (int a = 0; a < 3; ++a) {
        const int i = vertex_indices_[a];
        double diag_accumulation = 0.0;
        for (int j = 0; j < N; ++j) {
            diag_accumulation += compute_weight(i, j);
        }
        Af_[a][a] = diag_accumulation;
    }

    for (int a = 0; a < 3; ++a) {
        const int i = vertex_indices_[a];
        const auto& node = mesh.nodes[static_cast<size_t>(i)];
        bf_[a] = prob.f(node.x, node.y) * nodal_mass[static_cast<size_t>(i)];
    }
}

}