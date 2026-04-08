#pragma once

#include <string>
#include <vector>

namespace fem {

//  Robin slab analytic test — validates ∂u/∂n + k·u = g assembly.
//  P1 FEM reproduces the exact linear solution to machine precision.
struct RobinSlabTestResult {
    bool   passed            = false;
    double max_abs_err       = 0.0;
    double expected_exact_slope = 0.0;
    double got_slope         = 0.0;
};

//  MMS convergence study:  u_ex = sin(πx)sin(πy),  f = κ·2π²·u_ex
//  Domain [0,1]², Dirichlet=0 on ∂Ω.
//  Expected: rate_l2 ≈ 2,  rate_h1 ≈ 1 for P1 FEM.
struct MMSConvergenceLevel {
    int    n     = 0;
    int    dofs  = 0;
    double h     = 0.0;
    double l2    = 0.0;
    double h1    = 0.0;
    double rate_l2 = 0.0;
    double rate_h1 = 0.0;
};

struct MMSConvergenceResult {
    std::vector<MMSConvergenceLevel> levels;
    double avg_rate_l2 = 0.0;
    double avg_rate_h1 = 0.0;
    bool   passed      = false;
    std::string csv;
};

RobinSlabTestResult   run_robin_slab_self_test();
MMSConvergenceResult  run_mms_convergence_study(double kappa = 1.0);

} // namespace fem
