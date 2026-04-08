#include "fem_aitken_stress.h"

#include "math/fem/fem_error_analysis.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <random>
#include <vector>

namespace fem {

static double rand_symm(std::mt19937& rng) {
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    return dist(rng);
}

int run_aitken_stress(const AitkenStressConfig& cfg) {
    const double r = cfg.refinement_ratio;
    if (!(r > 1.0)) {
        return 2;
    }

    const double h3 = cfg.h3;
    const double h2 = h3 * r;
    const double h1 = h2 * r;

    std::ofstream out(cfg.out_csv_path);
    if (!out.is_open()) {
        return 3;
    }

    out.setf(std::ios::scientific);
    out.precision(16);

    out << "case_id,p_true,noise_rel,h1,h2,h3,q_inf_true,C_true,"
           "q1,q2,q3,valid,p_est,q_inf_est,err_fine_true,err_fine_est,"
           "ratio_12,ratio_23\n";

    std::mt19937 rng(cfg.seed);

    const std::vector<double> p_values = {0.25, 0.5, 1.0, 2.0, 3.0, 4.0};
    std::size_t case_id = 0;

    bool any_bad_noiseless = false;

    const auto noise_at = [&](int step) {
        if (!cfg.sweep_noise) return cfg.noise_rel;
        const int n = std::max(2, cfg.noise_rel_steps);
        const double t = double(step) / double(n - 1);
        return cfg.noise_rel_min + (cfg.noise_rel_max - cfg.noise_rel_min) * t;
    };

    const int noise_steps = cfg.sweep_noise ? std::max(2, cfg.noise_rel_steps) : 1;

    for (int ns = 0; ns < noise_steps; ++ns) {
        const double noise_rel = noise_at(ns);
        for (double p_true : p_values) {
            for (int i = 0; i < std::max(1, cfg.cases_per_p); ++i) {
                const double q_inf_true = (i % 2 == 0) ? 0.0 : (0.1 * (i + 1));
                const double C_true = ((i % 3) == 0 ? 1.0 : -1.0) * (0.25 + 0.05 * (i % 11));

                const auto model = [&](double h) {
                    const double term = C_true * std::pow(h, p_true);
                    const double noise = noise_rel * term * rand_symm(rng);
                    return q_inf_true + term + noise;
                };

                const double q1 = model(h1);
                const double q2 = model(h2);
                const double q3 = model(h3);

                const auto est = aitken_estimate_3<double>(q1, q2, q3, h1, h2, h3);

                const double err_fine_true = std::abs(q_inf_true - q3);
                const double err_fine_est = est.err_fine;

                const bool bad_noiseless = (noise_rel == 0.0) && est.valid &&
                                          (std::abs(est.p - p_true) > cfg.p_abs_tol_noiseless);

                if (bad_noiseless) {
                    any_bad_noiseless = true;
                }

                out << case_id++ << "," << p_true << "," << noise_rel << ","
                    << h1 << "," << h2 << "," << h3 << ","
                    << q_inf_true << "," << C_true << ","
                    << q1 << "," << q2 << "," << q3 << ","
                    << (est.valid ? 1 : 0) << "," << est.p << "," << est.q_inf << ","
                    << err_fine_true << "," << err_fine_est << ","
                    << est.ratio_12 << "," << est.ratio_23 << "\n";
            }
        }
    }

    out.flush();

    if (cfg.fail_on_bad_noiseless && !cfg.sweep_noise && cfg.noise_rel == 0.0 && any_bad_noiseless) {
        return 1;
    }

    return 0;
}

}
