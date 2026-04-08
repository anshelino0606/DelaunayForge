#pragma once
#include <vector>

namespace fem {

struct ErrorResults {
    bool valid = false;

    bool has_point = false;
    double px=0, py=0;
    double uh=0, uref=0, abs_err=0;

    double linf_nodes = 0.0; // node-sampled difference
    double l2 = 0.0;         // integrated difference
    double h1_semi = 0.0;    // integrated grad-difference (if available)
    bool   has_h1 = false;

    bool   has_energy = false;
    double energy = 0.0;

    bool   has_residual = false;
    double r_l2 = 0.0;
    double r_linf = 0.0;

    std::vector<float> elem_indicator;
    
    double global_indicator = 0.0;
};

} // namespace fem
