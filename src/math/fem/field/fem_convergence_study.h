#pragma once

#include "math/fem/fem_mesh.h"
#include "math/fem/fem_error_analysis.h"
#include "fem_reference_provider.h"
#include "fem_exact_field.h"
#include <vector>
#include <string>
#include <functional>

namespace fem {

enum class ErrorNorm {
    LInf,
    L2,
    H1Semi,
    H1Full,
    Energy
};

struct NormResult {
    ErrorNorm norm_type;
    double value = 0.0;
    bool valid = false;
    std::string description;
};

struct ConvergenceDataPoint {
    int level = 0;
    double h = 0.0;
    int dofs = 0;
    

    double linf = 0.0;
    double l2 = 0.0;
    double h1_semi = 0.0;
    double h1_full = 0.0;
    double energy = 0.0;
    double rel_l2 = 0.0;
    double rel_h1_semi = 0.0;
    double rel_h1_full = 0.0;
    

    double rate_linf = 0.0;
    double rate_l2 = 0.0;
    double rate_h1 = 0.0;
    double rate_energy = 0.0;
    
    bool has_h1 = false;
    bool has_energy = false;
    bool has_relative = false;
    

    double point_value = 0.0;
    bool has_point = false;
};

struct AitkenData {
    double h1 = 0.0, h2 = 0.0, h3 = 0.0;
    double q1 = 0.0, q2 = 0.0, q3 = 0.0;
    
    double q_inf = 0.0;
    double convergence_rate = 0.0;
    double error_estimate = 0.0;
    
    bool valid = false;
    std::string error_message;
};

struct ConvergenceStudyConfig {

    int min_level = 0;
    int max_level = 4;
    int ref_level = 6;
    ReferenceRefinementStrategy reference_refinement_strategy =
        ReferenceRefinementStrategy::UniformTriangulationSubdivision;
    

    bool use_exact_solution = true;
    bool compute_rates = true;
    bool export_data = true;
    

    bool compute_linf = true;
    bool compute_l2 = true;
    bool compute_h1 = true;
    bool compute_energy = true;

    bool remove_mean_offset = false;
    

    bool track_point = false;
    double point_x = 0.0;
    double point_y = 0.0;
};

struct ConvergenceStudyResults {
    std::vector<ConvergenceDataPoint> data;
    

    std::vector<AitkenData> aitken_linf;
    std::vector<AitkenData> aitken_l2;
    std::vector<AitkenData> aitken_point;
    

    double avg_rate_linf = 0.0;
    double avg_rate_l2 = 0.0;
    double avg_rate_h1 = 0.0;
    double avg_rate_energy = 0.0;
    
    bool valid = false;
    std::string error_message;
    

    std::string to_csv() const;
    std::string to_latex_table() const;
    std::string to_python_plot() const;
};


class ConvergenceStudyEngine {
public:
    ConvergenceStudyEngine() = default;
    

    ConvergenceStudyResults run_study(
        const IReferenceProvider* provider,
        const ConvergenceStudyConfig& config,
        const ExactSolution* exact = nullptr
    );
    

    ConvergenceDataPoint compute_level_error(
        const FEMMesh& mesh,
        const std::vector<double>& solution,
        const CRS* system_matrix,
        const ExactSolution* exact,
        int level
    );
    

    ConvergenceDataPoint compute_level_error_vs_reference(
        const FEMMesh& mesh,
        const std::vector<double>& solution,
        const FEMMesh& ref_mesh,
        const std::vector<double>& ref_solution,
        int level,
        bool remove_mean_offset
    );
    

    void compute_convergence_rates(std::vector<ConvergenceDataPoint>& data);
    

    AitkenData compute_aitken_extrapolation(
        const ConvergenceDataPoint& p1,
        const ConvergenceDataPoint& p2,
        const ConvergenceDataPoint& p3,
        ErrorNorm norm
    );

private:
    TriLocator locator_;
};

struct LocalErrorResult {
    double x = 0.0, y = 0.0;
    double uh = 0.0;
    double uex = 0.0;
    double abs_error = 0.0;
    
    double grad_uh_x = 0.0, grad_uh_y = 0.0;
    double grad_ex_x = 0.0, grad_ex_y = 0.0;
    double grad_error = 0.0;
    
    bool has_exact = false;
    bool has_gradient = false;
    bool point_found = false;
};

LocalErrorResult evaluate_local_error(
    const FEMMesh& mesh,
    const std::vector<double>& solution,
    double x, double y,
    const ExactSolution* exact = nullptr,
    TriLocator* locator = nullptr
);

}
