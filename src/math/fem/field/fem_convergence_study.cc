#include "fem_convergence_study.h"
#include "math/fem/fem_error_analysis.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace fem {

static std::string latex_sci_or_dash(double value, bool valid) {
    if (!valid) return "---";
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(2) << value;
    return ss.str();
}

std::string ConvergenceStudyResults::to_csv() const {
    std::ostringstream ss;
    ss.setf(std::ios::scientific);
    ss.precision(16);
    
    ss << "level,h,dofs,linf,l2,rel_l2,h1_semi,h1_full,rel_h1_semi,rel_h1_full,energy,rate_linf,rate_l2,rate_h1,rate_energy\n";
    
    for (const auto& pt : data) {
        ss << pt.level << ","
           << pt.h << ","
           << pt.dofs << ","
           << pt.linf << ","
           << pt.l2 << ","
           << pt.rel_l2 << ","
           << pt.h1_semi << ","
           << pt.h1_full << ","
           << pt.rel_h1_semi << ","
           << pt.rel_h1_full << ","
           << pt.energy << ","
           << pt.rate_linf << ","
           << pt.rate_l2 << ","
           << pt.rate_h1 << ","
           << pt.rate_energy << "\n";
    }
    
    return ss.str();
}

std::string ConvergenceStudyResults::to_latex_table() const {
    std::ostringstream ss;
    
    ss << "% LaTeX table for FEM convergence study\n";
    ss << "\\begin{table}[htbp]\n";
    ss << "\\centering\n";
    ss << "\\begin{tabular}{ccccccccc}\n";
    ss << "\\toprule\n";
    ss << "Level & $h$ & DOFs & $\\|e\\|_{L^\\infty}$ & $\\|e\\|_{L^2}$ & $\\|e\\|_{L^2}/\\|u\\|_{L^2}$ & $\\|e\\|_{W^{1,2}}$ & $\\|e\\|_{W^{1,2}}/\\|u\\|_{W^{1,2}}$ & Rate \\\\n";
    ss << "\\midrule\n";
    
    for (const auto& pt : data) {
        ss << pt.level << " & "
           << std::scientific << std::setprecision(2) << pt.h << " & "
           << pt.dofs << " & "
           << std::scientific << std::setprecision(2) << pt.linf << " & "
           << std::scientific << std::setprecision(2) << pt.l2 << " & "
           << latex_sci_or_dash(pt.rel_l2, pt.has_relative) << " & "
           << std::scientific << std::setprecision(2) << pt.h1_full << " & "
           << latex_sci_or_dash(pt.rel_h1_full, pt.has_relative);
        
        if (pt.rate_h1 > 0.0) {
            ss << " & " << std::fixed << std::setprecision(2) << pt.rate_h1;
        } else {
            ss << " & ---";
        }
        ss << " \\\\\n";
    }
    
    ss << "\\bottomrule\n";
    ss << "\\end{tabular}\n";
    ss << "\\caption{Convergence study results}\n";
    ss << "\\label{tab:convergence}\n";
    ss << "\\end{table}\n";
    
    return ss.str();
}

std::string ConvergenceStudyResults::to_python_plot() const {
    std::ostringstream ss;
    
    ss << "# Python plotting script for convergence study\n";
    ss << "import numpy as np\n";
    ss << "import matplotlib.pyplot as plt\n\n";
    
    ss << "# Data\n";
    ss << "h = np.array([";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << std::scientific << std::setprecision(6) << data[i].h;
    }
    ss << "])\n\n";
    
    ss << "linf = np.array([";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << std::scientific << std::setprecision(6) << data[i].linf;
    }
    ss << "])\n\n";
    
    ss << "l2 = np.array([";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << std::scientific << std::setprecision(6) << data[i].l2;
    }
    ss << "])\n\n";
    
    ss << "h1 = np.array([";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << std::scientific << std::setprecision(6) << data[i].h1_full;
    }
    ss << "])\n\n";
    
    ss << "# Plot\n";
    ss << "plt.figure(figsize=(10, 6))\n";
    ss << "plt.loglog(h, linf, 'o-', label='$L^\\infty$')\n";
    ss << "plt.loglog(h, l2, 's-', label='$L^2$')\n";
    ss << "plt.loglog(h, h1, '^-', label='$W^{1,2}$')\n";
    
    if (avg_rate_l2 > 0.0) {
        ss << "\n# Reference lines\n";
        ss << "plt.loglog(h, h[0] * (h/h[0])**" << std::fixed << std::setprecision(1) 
           << avg_rate_l2 << ", 'k--', alpha=0.5, label=f'$O(h^{" 
           << avg_rate_l2 << "})$')\n";
    }
    
    ss << "\nplt.xlabel('Mesh size $h$')\n";
    ss << "plt.ylabel('Error norm')\n";
    ss << "plt.title('FEM Convergence Study')\n";
    ss << "plt.legend()\n";
    ss << "plt.grid(True, alpha=0.3)\n";
    ss << "plt.tight_layout()\n";
    ss << "plt.savefig('convergence_study.pdf')\n";
    ss << "plt.show()\n";
    
    return ss.str();
}

// Convergence Study Engine Implementation

ConvergenceStudyResults ConvergenceStudyEngine::run_study(
    const IReferenceProvider* provider,
    const ConvergenceStudyConfig& config,
    const ExactSolution* exact
) {
    ConvergenceStudyResults results;
    
    if (!provider) {
        results.error_message = "No reference provider available";
        return results;
    }
    
    const bool use_exact = (config.use_exact_solution && exact && exact->has_u);
    
    // Build reference solution if needed
    FEMMesh ref_mesh;
    std::vector<double> ref_solution;
    
    if (!use_exact) {
        ReferenceSolveRequest req;
        req.refinement_level = config.ref_level;
        req.refinement_strategy = config.reference_refinement_strategy;
        ReferenceSolution ref_out;
        
        if (!provider->solve_reference(req, ref_out) || !ref_out.sol.is_ready()) {
            if (!ref_out.error_message.empty()) {
                results.error_message = ref_out.error_message;
            } else {
                results.error_message = "Failed to build reference solution at level " +
                                       std::to_string(config.ref_level);
            }
            return results;
        }
        
        ref_mesh = std::move(ref_out.mesh);
        ref_solution = std::move(ref_out.sol.solution_u);
        locator_.build(ref_mesh, 64);
    }
    
    // Solve at each refinement level
    for (int lvl = config.min_level; lvl <= config.max_level; ++lvl) {
        ReferenceSolveRequest req;
        req.refinement_level = lvl;
        req.refinement_strategy = config.reference_refinement_strategy;
        ReferenceSolution sol_out;
        
        if (!provider->solve_reference(req, sol_out) || !sol_out.sol.is_ready()) {
            if (!sol_out.error_message.empty()) {
                results.error_message = sol_out.error_message;
            } else {
                results.error_message = "Solve failed at level " + std::to_string(lvl);
            }
            break;
        }
        
        ConvergenceDataPoint pt;
        
        if (use_exact) {
            pt = compute_level_error(
                sol_out.mesh,
                sol_out.sol.solution_u,
                sol_out.has_sys ? &sol_out.sys.A : nullptr,
                exact,
                lvl
            );
        } else {
            pt = compute_level_error_vs_reference(
                sol_out.mesh,
                sol_out.sol.solution_u,
                ref_mesh,
                ref_solution,
                lvl,
                config.remove_mean_offset
            );
        }
        
        results.data.push_back(pt);
    }
    
    if (results.data.empty()) {
        results.error_message = "No data points computed";
        return results;
    }
    
    // Compute convergence rates
    if (config.compute_rates) {
        compute_convergence_rates(results.data);
        
        // Average rates (skip first entry which has no rate)
        if (results.data.size() > 1) {
            double sum_linf = 0.0;
            double sum_l2 = 0.0;
            double sum_h1 = 0.0;
            double sum_energy = 0.0;
            int count_linf = 0;
            int count_l2 = 0;
            int count_h1 = 0;
            int count_energy = 0;
            for (size_t i = 1; i < results.data.size(); ++i) {
                if (results.data[i].rate_linf > 0.0) {
                    sum_linf += results.data[i].rate_linf;
                    count_linf++;
                }
                if (results.data[i].rate_l2 > 0.0) {
                    sum_l2 += results.data[i].rate_l2;
                    count_l2++;
                }
                if (results.data[i].rate_h1 > 0.0) {
                    sum_h1 += results.data[i].rate_h1;
                    count_h1++;
                }
                if (results.data[i].rate_energy > 0.0) {
                    sum_energy += results.data[i].rate_energy;
                    count_energy++;
                }
            }
            if (count_linf > 0) {
                results.avg_rate_linf = sum_linf / count_linf;
            }
            if (count_l2 > 0) {
                results.avg_rate_l2 = sum_l2 / count_l2;
            }
            if (count_h1 > 0) {
                results.avg_rate_h1 = sum_h1 / count_h1;
            }
            if (count_energy > 0) {
                results.avg_rate_energy = sum_energy / count_energy;
            }
        }
    }
    
    // Aitken extrapolation (requires at least 3 levels)
    if (results.data.size() >= 3) {
        for (size_t i = 0; i + 2 < results.data.size(); ++i) {
            results.aitken_l2.push_back(
                compute_aitken_extrapolation(
                    results.data[i], results.data[i+1], results.data[i+2],
                    ErrorNorm::L2
                )
            );
            
            results.aitken_linf.push_back(
                compute_aitken_extrapolation(
                    results.data[i], results.data[i+1], results.data[i+2],
                    ErrorNorm::LInf
                )
            );
        }
    }
    
    results.valid = true;
    return results;
}

ConvergenceDataPoint ConvergenceStudyEngine::compute_level_error(
    const FEMMesh& mesh,
    const std::vector<double>& solution,
    const CRS* system_matrix,
    const ExactSolution* exact,
    int level
) {
    ConvergenceDataPoint pt;
    pt.level = level;
    pt.h = mesh_h_max_edge<double>(mesh);
    pt.dofs = static_cast<int>(mesh.dof_count());
    
    if (!exact || !exact->has_u) {
        return pt;
    }
    
    auto err = compute_error_metrics<double>(mesh, solution, exact, nullptr, system_matrix);
    
    pt.linf = err.linf_nodes;
    pt.l2 = err.l2;
    pt.h1_semi = err.h1_semi;
    pt.h1_full = err.h1_full;
    pt.energy = err.energy_A;
    pt.rel_l2 = err.l2_rel;
    pt.rel_h1_semi = err.h1_semi_rel;
    pt.rel_h1_full = err.h1_full_rel;
    pt.has_h1 = err.has_grad;
    pt.has_energy = err.has_energy;
    pt.has_relative = err.has_relative;
    
    return pt;
}

ConvergenceDataPoint ConvergenceStudyEngine::compute_level_error_vs_reference(
    const FEMMesh& mesh,
    const std::vector<double>& solution,
    const FEMMesh& ref_mesh,
    const std::vector<double>& ref_solution,
    int level,
    bool remove_mean_offset
) {
    ConvergenceDataPoint pt;
    pt.level = level;
    pt.h = mesh_h_max_edge<double>(mesh);
    pt.dofs = static_cast<int>(mesh.dof_count());
    
    auto err = compute_error_vs_reference<double>(
        mesh, solution, ref_mesh, ref_solution, &locator_, remove_mean_offset
    );
    
    pt.linf = err.linf_nodes;
    pt.l2 = err.l2;
    pt.h1_semi = err.h1_semi;
    pt.h1_full = std::sqrt(pt.l2 * pt.l2 + pt.h1_semi * pt.h1_semi);
    pt.has_h1 = err.has_grad;
    pt.has_energy = false;  // Reference solution doesn't provide energy norm
    
    return pt;
}

void ConvergenceStudyEngine::compute_convergence_rates(
    std::vector<ConvergenceDataPoint>& data
) {
    if (data.size() < 2) return;
    
    for (size_t i = 1; i < data.size(); ++i) {
        const auto& prev = data[i-1];
        auto& curr = data[i];  // Non-const to allow modification
        
        const double h_ratio = prev.h / std::max(curr.h, 1e-16);
        const double log_h = std::log(h_ratio);
        
        if (log_h > 1e-10) {
            // L-infinity rate
            if (prev.linf > 1e-16 && curr.linf > 1e-16) {
                curr.rate_linf = std::log(prev.linf / curr.linf) / log_h;
            }
            
            // L2 rate
            if (prev.l2 > 1e-16 && curr.l2 > 1e-16) {
                curr.rate_l2 = std::log(prev.l2 / curr.l2) / log_h;
            }
            
            // W1,2 / full H1 rate
            if (prev.h1_full > 1e-16 && curr.h1_full > 1e-16) {
                curr.rate_h1 = std::log(prev.h1_full / curr.h1_full) / log_h;
            }
            
            // Energy rate
            if (prev.energy > 1e-16 && curr.energy > 1e-16 && prev.has_energy && curr.has_energy) {
                curr.rate_energy = std::log(prev.energy / curr.energy) / log_h;
            }
        }
    }
}

AitkenData ConvergenceStudyEngine::compute_aitken_extrapolation(
    const ConvergenceDataPoint& p1,
    const ConvergenceDataPoint& p2,
    const ConvergenceDataPoint& p3,
    ErrorNorm norm
) {
    AitkenData result;
    result.h1 = p1.h;
    result.h2 = p2.h;
    result.h3 = p3.h;
    
    // Select quantity based on norm
    switch (norm) {
        case ErrorNorm::LInf:
            result.q1 = p1.linf;
            result.q2 = p2.linf;
            result.q3 = p3.linf;
            break;
        case ErrorNorm::L2:
            result.q1 = p1.l2;
            result.q2 = p2.l2;
            result.q3 = p3.l2;
            break;
        case ErrorNorm::H1Semi:
            result.q1 = p1.h1_semi;
            result.q2 = p2.h1_semi;
            result.q3 = p3.h1_semi;
            break;
        case ErrorNorm::H1Full:
            result.q1 = p1.h1_full;
            result.q2 = p2.h1_full;
            result.q3 = p3.h1_full;
            break;
        case ErrorNorm::Energy:
            result.q1 = p1.energy;
            result.q2 = p2.energy;
            result.q3 = p3.energy;
            break;
        default:
            result.error_message = "Unsupported norm type";
            return result;
    }
    
    // Use existing aitken_estimate_3 function
    auto est = aitken_estimate_3<double>(
        result.q1, result.q2, result.q3,
        result.h1, result.h2, result.h3
    );
    
    if (!est.valid) {
        result.error_message = "Aitken extrapolation failed - check mesh refinement ratio";
        return result;
    }
    
    result.q_inf = est.q_inf;
    result.convergence_rate = est.p;
    result.error_estimate = est.err_fine;
    result.valid = true;
    
    return result;
}

// Local Error Analysis

LocalErrorResult evaluate_local_error(
    const FEMMesh& mesh,
    const std::vector<double>& solution,
    double x, double y,
    const ExactSolution* exact,
    TriLocator* locator
) {
    LocalErrorResult result;
    result.x = x;
    result.y = y;
    
    // Evaluate numerical solution
    double uh_val = 0.0;
    int tri_id = -1;
    
    if (!eval_p1_at<double>(mesh, solution, x, y, uh_val, &tri_id, locator)) {
        return result;  // Point not found in mesh
    }
    
    result.uh = uh_val;
    result.point_found = true;
    
    // Compute gradient if triangle found
    if (tri_id >= 0 && tri_id < (int)mesh.elems.size()) {
        const auto& elem = mesh.elems[tri_id];
        const auto& p0 = mesh.nodes[elem.v[0]];
        const auto& p1 = mesh.nodes[elem.v[1]];
        const auto& p2 = mesh.nodes[elem.v[2]];
        
        double grad_phi[3][2];
        compute_p1_gradients<double>(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, grad_phi);
        
        const double u0 = solution[elem.v[0]];
        const double u1 = solution[elem.v[1]];
        const double u2 = solution[elem.v[2]];
        
        result.grad_uh_x = u0 * grad_phi[0][0] + u1 * grad_phi[1][0] + u2 * grad_phi[2][0];
        result.grad_uh_y = u0 * grad_phi[0][1] + u1 * grad_phi[1][1] + u2 * grad_phi[2][1];
    }
    
    // Compare with exact solution if available
    if (exact && exact->has_u) {
        result.uex = exact->u_exact(x, y);
        result.abs_error = std::abs(result.uex - result.uh);
        result.has_exact = true;
        
        if (exact->has_grad && exact->grad_exact) {
            double gx = 0.0, gy = 0.0;
            if (exact->grad_exact(x, y, gx, gy)) {
                result.grad_ex_x = gx;
                result.grad_ex_y = gy;
                
                const double dx = result.grad_ex_x - result.grad_uh_x;
                const double dy = result.grad_ex_y - result.grad_uh_y;
                result.grad_error = std::sqrt(dx*dx + dy*dy);
                result.has_gradient = true;
            }
        }
    }
    
    return result;
}

} // namespace fem
