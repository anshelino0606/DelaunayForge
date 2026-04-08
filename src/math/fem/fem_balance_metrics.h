#pragma once

#include <span>
#include <string>
#include <vector>
#include "math/differential_equation.h"
#include "math/fem/fem_mesh.h"

namespace fem {

//  Balance / flux metrics for a single (mesh, solution) pair.
//
//  Equation: −div(a·∇u) + c·u = f
//  Sign convention:
//    q_out_i = outward flux leaving domain through boundary piece i
//    Robin  : q = k·u − g     (from ∂u/∂n + k·u = g)
//    Neumann: q = −gN
//    Dirichlet: q = −a·(∇u·n) (reconstructed from element gradient)
//
//  Conservation residual (should → 0):
//    R = (Q_outer + Q_inner) + ∫cu dx − ∫f dx

struct BalanceMetrics {
    int    level = 0;
    int    dofs  = 0;
    double h     = 0.0;

    // Bounding box
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;

    // Outer boundary flux per side (positive = leaving domain)
    double flux_left   = 0.0;
    double flux_right  = 0.0;
    double flux_bottom = 0.0;
    double flux_top    = 0.0;
    double flux_outer  = 0.0;

    // Inner (pore-wall) exchange integral
    double inner_exchange          = 0.0;
    double inner_perimeter         = 0.0;
    double inner_exchange_per_length = 0.0;

    // Effective conductivity:  k_eff = |Q_right| / (Δu / width)
    double k_eff = 0.0;

    // Domain integrals
    double domain_area  = 0.0;
    double integral_f   = 0.0;
    double integral_cu  = 0.0;

    // Conservation residual
    double conservation_residual = 0.0;

    // Max-principle bounds
    double u_min = 0.0;
    double u_max = 0.0;
};

struct BalanceMetricsConfig {
    double outer_classify_tol = 0.0; // 0 = auto (1e-9 * bbox)
};

BalanceMetrics compute_balance_metrics(
    const FEMMesh& mesh,
    std::span<const double> u,
    const Coefficient<double>& a,
    const Coefficient<double>& c,
    const Coefficient<double>& f,
    const BalanceMetricsConfig& cfg = {}
);

// Format metrics as CSV header + row
std::string balance_metrics_csv_header();
std::string balance_metrics_csv_row(const BalanceMetrics& m);

} // namespace fem
