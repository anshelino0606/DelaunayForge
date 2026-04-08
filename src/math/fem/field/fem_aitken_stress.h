#pragma once

#include <cstdint>
#include <string>

namespace fem {

struct AitkenStressConfig {
    std::string out_csv_path = "aitken_stress.csv";

    double h3 = 0.125;
    double refinement_ratio = 2.0;

    int cases_per_p = 50;

    bool sweep_noise = false;
    double noise_rel = 0.0;
    double noise_rel_min = 0.0;
    double noise_rel_max = 0.02;
    int noise_rel_steps = 11;
    std::uint32_t seed = 1;

    bool fail_on_bad_noiseless = true;
    double p_abs_tol_noiseless = 1e-8;
};

int run_aitken_stress(const AitkenStressConfig& cfg);

}
