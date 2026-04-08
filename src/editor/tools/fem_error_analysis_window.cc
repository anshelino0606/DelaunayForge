#include "fem_error_analysis_window.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "math/pde/pde_component.h"
#include "geom/delaunay_types.h"

#include <imgui/imgui.h>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <random>

#include "log_categories.h"
#include "math/fem/fem_balance_metrics.h"
#include "math/fem/fem_energy_metrics.h"
#include "math/fem/fem_self_tests.h"
#include "math/fem/fem_problem.h"
#include "math/fem/fem_assemblers_p1.h"

namespace fem {

using PointT = std::remove_cv_t<std::remove_reference_t<
    decltype(std::declval<DelaunayTriangulationResult>().points[0])>>;

static inline glm::dvec2 p2(const PointT& P) {
    return glm::dvec2((double)P.x(), (double)P.y());
}

static inline bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(content.data(), (std::streamsize)content.size());
    return f.good();
}

static inline const IReferenceProvider* resolve_reference_provider(
    const FEMErrorAnalysisWindow::DrawInfo& info,
    FEMReferenceProvider& fallback,
    bool& using_fallback
) {
    using_fallback = false;
    if (info.ref) return info.ref;
    if (!info.pde || !info.mesh) return nullptr;
    fallback = FEMReferenceProvider(info.pde, info.mesh);
    using_fallback = true;
    return &fallback;
}

static inline std::string export_prefix_to_pathsafe_prefix(const std::string& prefix) {
    if (prefix.empty()) return std::string("fem_error_analysis");
    return prefix;
}

static inline bool sample_points_in_mesh(
    const FEMMesh& mesh,
    int count,
    unsigned int seed,
    std::vector<std::pair<double,double>>& out_points
) {
    out_points.clear();
    if (count <= 0) return false;
    if (mesh.elems.empty() || mesh.nodes.empty()) return false;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> tri_dist(0, (int)mesh.elems.size() - 1);
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    out_points.reserve((size_t)count);
    for (int i = 0; i < count; ++i) {
        const int tid = tri_dist(rng);
        const auto& E = mesh.elems[(size_t)tid];
        const auto& P0 = mesh.nodes[(size_t)E.v[0]];
        const auto& P1 = mesh.nodes[(size_t)E.v[1]];
        const auto& P2 = mesh.nodes[(size_t)E.v[2]];

        const double r1 = uni(rng);
        const double r2 = uni(rng);
        const double s = std::sqrt(r1);
        const double b0 = 1.0 - s;
        const double b1 = s * (1.0 - r2);
        const double b2 = s * r2;

        const double x = b0 * P0.x + b1 * P1.x + b2 * P2.x;
        const double y = b0 * P0.y + b1 * P1.y + b2 * P2.y;
        out_points.emplace_back(x, y);
    }
    return true;
}


void FEMErrorAnalysisWindow::ensure_cache_(const PlanarMeshComponent& mesh) {
    const auto& R = mesh.triangulation_result();
    const std::size_t pc = R.points.size();
    const std::size_t tc = R.triangles.size();

    if (cached_mesh_ == &mesh && cached_point_count_ == pc && cached_tri_count_ == tc) return;

    cached_mesh_ = &mesh;
    cached_point_count_ = pc;
    cached_tri_count_ = tc;

    cached_fem_ = mesh.build_fem_mesh();
    locator_.build(cached_fem_);

    // Invalidate cached results
    has_local_error_ = false;
    has_global_error_ = false;
}

bool FEMErrorAnalysisWindow::try_fill_exact_(ExactSolution& out, const PDEComponent* pde) const {
    out.has_u = false;
    out.has_grad = false;
    
    if (!pde || !pde->has_exact_solution()) return false;
    
    out.u_exact = [pde](double x, double y) -> double {
        double u = 0.0, ux = 0.0, uy = 0.0;
        return pde->get_exact_solution(x, y, u, &ux, &uy) ? u : 0.0;
    };
    
    out.grad_exact = [pde](double x, double y, double& ux, double& uy) -> bool {
        double u = 0.0;
        return pde->get_exact_solution(x, y, u, &ux, &uy);
    };
    
    out.has_u = true;
    out.has_grad = true;
    return true;
}

bool FEMErrorAnalysisWindow::selection_point_(
    const DelaunayTriangulationResult& R,
    const CanvasInspector::Selection& sel,
    double& x, double& y) const 
{
    x = 0.0; y = 0.0;
    if (!sel.valid()) return false;

    if (sel.kind == CanvasInspector::Kind::Vertex) {
        if (sel.id < 0 || (size_t)sel.id >= R.points.size()) return false;
        x = R.points[sel.id].x();
        y = R.points[sel.id].y();
        return true;
    }

    if (sel.kind == CanvasInspector::Kind::Edge) {
        if (sel.id < 0 || (size_t)sel.id >= R.edges.size()) return false;
        const auto& E = R.edges[sel.id];
        if ((size_t)E.a >= R.points.size() || (size_t)E.b >= R.points.size()) return false;
        x = 0.5 * (R.points[E.a].x() + R.points[E.b].x());
        y = 0.5 * (R.points[E.a].y() + R.points[E.b].y());
        return true;
    }

    if (sel.kind == CanvasInspector::Kind::Triangle) {
        if (sel.id < 0 || (size_t)sel.id >= R.triangles.size()) return false;
        const auto& T = R.triangles[sel.id];
        if (!T.valid) return false;
        if ((size_t)T.v[0] >= R.points.size() || 
            (size_t)T.v[1] >= R.points.size() || 
            (size_t)T.v[2] >= R.points.size()) return false;
        x = (R.points[T.v[0]].x() + R.points[T.v[1]].x() + R.points[T.v[2]].x()) / 3.0;
        y = (R.points[T.v[0]].y() + R.points[T.v[1]].y() + R.points[T.v[2]].y()) / 3.0;
        return true;
    }

    return false;
}

void FEMErrorAnalysisWindow::draw_section_mesh_info_(const DrawInfo& info) {
    const double h = mesh_h_max_edge<double>(cached_fem_);
    const int dofs = (int)info.sol->solution_u.size();
    
    ImGui::SeparatorText("Current Mesh");
    ImGui::Text("h (max edge): %.6g", h);
    ImGui::Text("DOFs: %d", dofs);
    ImGui::Text("Elements: %d", (int)cached_fem_.elems.size());
    
    ExactSolution exact;
    const bool has_exact = try_fill_exact_(exact, info.pde);
    
    if (has_exact) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "✓ Exact solution available");
    } else {
        ImGui::TextDisabled("No exact solution");
    }
    
    FEMReferenceProvider fb(info.pde, info.mesh);
    bool using_fb = false;
    const IReferenceProvider* rp = resolve_reference_provider(info, fb, using_fb);
    if (rp) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                           using_fb ? "✓ Reference provider available (mesh fallback)"
                                    : "✓ Reference provider available");
    } else {
        ImGui::TextDisabled("No reference provider");
    }
}

void FEMErrorAnalysisWindow::draw_section_local_analysis_(const DrawInfo& info) {
    ImGui::SeparatorText("Local Point Analysis");
    
    double px = 0.0, py = 0.0;
    const bool has_sel = selection_point_(info.mesh->triangulation_result(), info.sel, px, py);
    
    // Update tracking point from selection if not locked
    if (has_sel && !probe_locked_) {
        probe_x_ = px;
        probe_y_ = py;
    }
    
    // Lock/unlock controls
    ImGui::Spacing();
    if (probe_locked_) {
        ImGui::Text("🔒 Locked at (%.9f, %.9f)", probe_x_, probe_y_);
        if (ImGui::Button("Unlock")) {
            probe_locked_ = false;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(will follow selection)");
    } else {
        if (has_sel) {
            ImGui::Text("📍 Selection: (%.9f, %.9f)", px, py);
            ImGui::SameLine();
            if (ImGui::Button("Lock")) {
                probe_locked_ = true;
                probe_x_ = px;
                probe_y_ = py;
            }
        } else {
            ImGui::TextDisabled("Click vertex/edge/triangle to select probe point");
        }
    }
    
    // Manual input
    ImGui::InputDouble("X##probe", &probe_x_, 0.0, 0.0, "%.9f");
    ImGui::InputDouble("Y##probe", &probe_y_, 0.0, 0.0, "%.9f");
    
    // Evaluate at probe
    ImGui::Spacing();
    const bool has_probe = probe_locked_ || has_sel;
    if (has_probe && ImGui::Button("Evaluate at point")) {
        const double eval_x = probe_locked_ ? probe_x_ : px;
        const double eval_y = probe_locked_ ? probe_y_ : py;
        
        ExactSolution exact;
        const bool has_exact = try_fill_exact_(exact, info.pde);
        
        current_local_error_ = evaluate_local_error(
            cached_fem_, info.sol->solution_u,
            eval_x, eval_y,
            has_exact ? &exact : nullptr,
            &locator_
        );
        
        has_local_error_ = true;
    }
    
    if (has_local_error_) {
        ImGui::Spacing();
        ImGui::Separator();
        if (current_local_error_.point_found) {
            ImGui::Text("Point: (%.6g, %.6g)", current_local_error_.x, current_local_error_.y);
            ImGui::Text("u_h = %.12g", current_local_error_.uh);
            
            if (current_local_error_.has_gradient) {
                ImGui::Text("∇u_h = (%.6g, %.6g)", 
                           current_local_error_.grad_uh_x, 
                           current_local_error_.grad_uh_y);
            }
            
            if (current_local_error_.has_exact) {
                ImGui::Spacing();
                ImGui::Text("u_ex = %.12g", current_local_error_.uex);
                ImGui::Text("|error| = %.6e", current_local_error_.abs_error);
                
                if (current_local_error_.has_gradient) {
                    ImGui::Text("∇u_ex = (%.6g, %.6g)", 
                               current_local_error_.grad_ex_x, 
                               current_local_error_.grad_ex_y);
                    ImGui::Text("|∇error| = %.6e", current_local_error_.grad_error);
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Point outside domain");
        }
    }
}

void FEMErrorAnalysisWindow::draw_section_global_norms_(const DrawInfo& info) {
    ImGui::SeparatorText("Global Error Norms");
    
    ExactSolution exact;
    const bool has_exact = try_fill_exact_(exact, info.pde);
    
    if (!has_exact) {
        ImGui::TextDisabled("Requires exact solution from PDE preset");
        return;
    }
    
    if (ImGui::Button("Compute Global Error")) {
        global_error_ = compute_error_metrics<double>(
            cached_fem_, info.sol->solution_u, &exact, nullptr, info.A);
        has_global_error_ = true;
    }
    
    if (has_global_error_) {
        ImGui::Spacing();
        ImGui::Separator();
        
        ImGui::Text("L∞ (nodal):  %.6e", global_error_.linf_nodes);
        ImGui::Text("L²:          %.6e", global_error_.l2);
        
        if (global_error_.has_grad) {
            ImGui::Text("H¹ semi:     %.6e", global_error_.h1_semi);
            const double h1_full = std::sqrt(
                global_error_.l2 * global_error_.l2 + 
                global_error_.h1_semi * global_error_.h1_semi);
            ImGui::Text("H¹ full:     %.6e", h1_full);
        }
        
        if (global_error_.has_energy) {
            ImGui::Text("Energy:      %.6e", global_error_.energy_A);
        }
    }
}

void FEMErrorAnalysisWindow::draw_section_convergence_study_(const DrawInfo& info) {
    ImGui::SeparatorText("Convergence Study");

    FEMReferenceProvider fb(info.pde, info.mesh);
    bool using_fb = false;
    const IReferenceProvider* rp = resolve_reference_provider(info, fb, using_fb);
    if (!rp) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "⚠ No reference provider");
        return;
    }
    
    ImGui::Spacing();
    
    // Configuration
    ExactSolution exact;
    const bool has_exact = try_fill_exact_(exact, info.pde);
    
    if (!has_exact) {
        study_config_.use_exact_solution = false;
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox("Use exact solution", &study_config_.use_exact_solution);
    if (!has_exact) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(not available)");
    }
    
    ImGui::InputInt("Min level", &study_config_.min_level);
    study_config_.min_level = std::max(0, study_config_.min_level);
    
    ImGui::InputInt("Max level", &study_config_.max_level);
    study_config_.max_level = std::max(study_config_.min_level, 
                                       std::min(study_config_.max_level, 10));

    ImGui::TextDisabled("Note: reference solves use uniform refinement; large base meshes can explode in size quickly.");
    
    if (!study_config_.use_exact_solution) {
        ImGui::InputInt("Reference level (finest)", &study_config_.ref_level);
        study_config_.ref_level = std::max(study_config_.max_level + 1, study_config_.ref_level);

        ImGui::Checkbox("Remove mean offset (zero-mean compare)", &study_config_.remove_mean_offset);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "For problems with a constant nullspace (e.g. pure Neumann),\n"
                "solutions can differ by a constant. This compares u - mean(u)."
            );
        }
    }
    else {
        study_config_.remove_mean_offset = false;
    }
    
    ImGui::Checkbox("Track probe point", &study_config_.track_point);
    if (study_config_.track_point) {
        if (!probe_locked_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "⚠ Lock a point first!");
        } else {
            study_config_.point_x = probe_x_;
            study_config_.point_y = probe_y_;
        }
    }
    
    ImGui::Spacing();
    
    // Run study
    if (ImGui::Button("Run Convergence Study", ImVec2(200, 0))) {
        study_running_ = true;
        if (study_config_.use_exact_solution && has_exact) {
            // For exact/manufactured solutions on domains with holes (e.g., Sierpinski carpet),
            // enforce Dirichlet values on ALL boundary edges from u_exact.
            FEMReferenceProviderExactDirichlet exact_bc_provider(info.pde, info.mesh, exact.u_exact);
            study_results_ = study_engine_.run_study(
                &exact_bc_provider,
                study_config_,
                &exact
            );
        } else {
            study_results_ = study_engine_.run_study(
                rp,
                study_config_,
                has_exact ? &exact : nullptr
            );
        }
        study_running_ = false;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear Results")) {
        study_results_ = ConvergenceStudyResults{};
    }
    
    // Display results
    if (!study_results_.error_message.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), 
                          "Error: %s", study_results_.error_message.c_str());
    }
    
    if (study_results_.valid && !study_results_.data.empty()) {
        ImGui::Spacing();
        ImGui::Text("Converge data: %d levels", (int)study_results_.data.size());
        
        if (study_results_.avg_rate_l2 > 0.0) {
            ImGui::Text("Average L² rate: %.3f", study_results_.avg_rate_l2);
        }
        if (study_results_.avg_rate_h1 > 0.0) {
            ImGui::Text("Average H¹ rate: %.3f", study_results_.avg_rate_h1);
        }
        
        ImGui::Spacing();
        
        // Table
        if (ImGui::BeginTable("ConvergenceTable", 8, 
                             ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                             ImGuiTableFlags_ScrollY, ImVec2(0, 250))) {
            
            ImGui::TableSetupColumn("Lvl", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("h", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("DOFs", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("L∞", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("L²", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Rate", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("H¹", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Rate", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableHeadersRow();
            
            for (const auto& pt : study_results_.data) {
                ImGui::TableNextRow();
                
                ImGui::TableNextColumn();
                ImGui::Text("%d", pt.level);
                
                ImGui::TableNextColumn();
                ImGui::Text("%.3e", pt.h);
                
                ImGui::TableNextColumn();
                ImGui::Text("%d", pt.dofs);
                
                ImGui::TableNextColumn();
                ImGui::Text("%.3e", pt.linf);
                
                ImGui::TableNextColumn();
                ImGui::Text("%.3e", pt.l2);
                
                ImGui::TableNextColumn();
                if (pt.rate_l2 > 0.0) {
                    ImGui::Text("%.2f", pt.rate_l2);
                } else {
                    ImGui::TextDisabled("—");
                }
                
                ImGui::TableNextColumn();
                if (pt.has_h1) {
                    ImGui::Text("%.3e", pt.h1_semi);
                } else {
                    ImGui::TextDisabled("—");
                }
                
                ImGui::TableNextColumn();
                if (pt.rate_h1 > 0.0) {
                    ImGui::Text("%.2f", pt.rate_h1);
                } else {
                    ImGui::TextDisabled("—");
                }
            }
            
            ImGui::EndTable();
        }
        
        ImGui::Spacing();
        if (ImGui::Button("Copy CSV")) {
            ImGui::SetClipboardText(study_results_.to_csv().c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Save CSV")) {
            if (write_file(export_path_ + "_convergence.csv", study_results_.to_csv())) {
                ImGui::OpenPopup("ExportSuccess");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Export LaTeX")) {
            if (write_file(export_path_ + "_table.tex", study_results_.to_latex_table())) {
                ImGui::OpenPopup("ExportSuccess");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Export Python")) {
            if (write_file(export_path_ + "_plot.py", study_results_.to_python_plot())) {
                ImGui::OpenPopup("ExportSuccess");
            }
        }
        
        if (ImGui::BeginPopupModal("ExportSuccess", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("File exported successfully");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
}

void FEMErrorAnalysisWindow::draw_section_aitken_analysis_(const DrawInfo& info) {
    ImGui::SeparatorText("Richardson-Aitken");
    ImGui::TextDisabled("Lock point → Solve → Capture → Refine → repeat 3x");
    
    if (!probe_locked_) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), 
                          "⚠ Lock a probe point first for point-wise Aitken");
        ImGui::TextWrapped("(Global norms will still be captured)");
    }
    
    ImGui::Spacing();
    
    // Controls
    if (ImGui::Button("Clear Captures")) {
        for (auto& c : aitken_captures_) c = AitkenCapture{};
        aitken_status_message_.clear();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Capture Current Solution")) {
        const double h = mesh_h_max_edge<double>(cached_fem_);
        const int dofs = cached_fem_.dof_count();
        
        // Check for duplicate mesh size
        bool is_duplicate = false;
        for (const auto& c : aitken_captures_) {
            if (c.valid && std::abs(c.h - h) < 1e-12 * std::max(h, c.h)) {
                is_duplicate = true;
                break;
            }
        }
        
        if (is_duplicate) {
            aitken_status_message_ = "⚠ Mesh size unchanged! Refine mesh before capturing.";
        } else {
            AitkenCapture new_cap;
            new_cap.valid = true;
            new_cap.h = h;
            new_cap.dofs = dofs;
            new_cap.mesh = cached_fem_;
            new_cap.solution = info.sol->solution_u;
            
            // Point value
            if (probe_locked_) {
                double pval = 0.0;
                if (eval_p1_at<double>(cached_fem_, info.sol->solution_u,
                                      probe_x_, probe_y_, pval, nullptr, &locator_)) {
                    new_cap.point_value = pval;
                }
            }
            
            ExactSolution exact;
            const bool has_exact = try_fill_exact_(exact, info.pde);
            if (has_exact) {
                auto err = compute_error_metrics<double>(
                    cached_fem_, info.sol->solution_u, &exact, nullptr, info.A);
                new_cap.linf = err.linf_nodes;
                new_cap.l2 = err.l2;
                new_cap.h1_semi = err.h1_semi;
            }
            
            // Shift captures
            aitken_captures_[0] = aitken_captures_[1];
            aitken_captures_[1] = aitken_captures_[2];
            aitken_captures_[2] = new_cap;
            
            aitken_status_message_ = "✓ Captured level with h=" + std::to_string(h);
        }
    }
    
    if (!aitken_status_message_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", aitken_status_message_.c_str());
    }
    
    ImGui::Spacing();
    ImGui::Text("Captured solutions:");
    
    bool all_valid = true;
    for (int i = 0; i < 3; ++i) {
        const auto& c = aitken_captures_[i];
        if (!c.valid) {
            ImGui::TextDisabled("  [%d]: empty", i + 1);
            all_valid = false;
        } else {
            ImGui::Text("  [%d]: h=%.5g, dofs=%d", i + 1, c.h, c.dofs);
            if (probe_locked_) {
                ImGui::SameLine();
                ImGui::Text("q=%.9g", c.point_value);
            }
        }
    }
    
    if (all_valid) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Richardson-Aitken Results:");
        
        const bool proper_refinement = 
            (aitken_captures_[0].h > aitken_captures_[1].h) &&
            (aitken_captures_[1].h > aitken_captures_[2].h);
        
        if (!proper_refinement) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), 
                              "⚠ Meshes not properly refined (need h₁ > h₂ > h₃)");
        } else {
            if (probe_locked_) {
                auto pt_aitken = aitken_estimate_3<double>(
                    aitken_captures_[0].point_value,
                    aitken_captures_[1].point_value,
                    aitken_captures_[2].point_value,
                    aitken_captures_[0].h,
                    aitken_captures_[1].h,
                    aitken_captures_[2].h
                );
                
                if (pt_aitken.valid) {
                    ImGui::Text("Point-wise:");
                    ImGui::Text("  Order p ≈ %.4g", pt_aitken.p);
                    ImGui::Text("  q_∞ ≈ %.12g", pt_aitken.q_inf);
                    ImGui::Text("  Est. error ≈ %.6e", pt_aitken.err_fine);
                }
            }
            
            // L2 Aitken (if exact available)
            if (aitken_captures_[0].l2 > 0.0) {
                auto l2_aitken = aitken_estimate_3<double>(
                    aitken_captures_[0].l2,
                    aitken_captures_[1].l2,
                    aitken_captures_[2].l2,
                    aitken_captures_[0].h,
                    aitken_captures_[1].h,
                    aitken_captures_[2].h
                );
                
                if (l2_aitken.valid && l2_aitken.p > 0) {
                    ImGui::Spacing();
                    ImGui::Text("L² error:");
                    ImGui::Text("  Order p ≈ %.4g", l2_aitken.p);
                }
            }
            
            // Global comparison (vs finest mesh)
            TriLocator loc_fine;
            loc_fine.build(aitken_captures_[2].mesh);
            
            auto e_01 = compute_error_vs_reference<double>(
                aitken_captures_[0].mesh, aitken_captures_[0].solution,
                aitken_captures_[2].mesh, aitken_captures_[2].solution, &loc_fine);
            
            auto e_12 = compute_error_vs_reference<double>(
                aitken_captures_[1].mesh, aitken_captures_[1].solution,
                aitken_captures_[2].mesh, aitken_captures_[2].solution, &loc_fine);
            
            if (e_01.valid && e_12.valid) {
                ImGui::Spacing();
                ImGui::Text("Global L² (vs finest):");
                ImGui::Text("  ||u₀ - u₂||_L² = %.6e", e_01.l2);
                ImGui::Text("  ||u₁ - u₂||_L² = %.6e", e_12.l2);
                
                if (e_01.l2 > 1e-16 && e_12.l2 > 1e-16) {
                    const double h_ratio = aitken_captures_[0].h / aitken_captures_[1].h;
                    const double p_global = std::log(e_01.l2 / e_12.l2) / std::log(h_ratio);
                    ImGui::Text("  → Rate p ≈ %.4g", p_global);
                }
            }
        }
    }
}

void FEMErrorAnalysisWindow::draw_section_aitken_stress_(const DrawInfo& info) {
    ImGui::SeparatorText("Aitken Stress (selected PDE)");

    FEMReferenceProvider fb(info.pde, info.mesh);
    bool using_fb = false;
    const IReferenceProvider* rp = resolve_reference_provider(info, fb, using_fb);
    if (!rp) {
        ImGui::TextDisabled("No reference provider available.");
        return;
    }

    ImGui::TextDisabled("Solves multiple refinement levels for the current selection and exports CSVs.");

    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("min level", &stress_cfg_.min_level);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("max level", &stress_cfg_.max_level);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("ref level", &stress_cfg_.ref_level);

    stress_cfg_.min_level = std::max(0, stress_cfg_.min_level);
    stress_cfg_.max_level = std::max(stress_cfg_.min_level, stress_cfg_.max_level);
    stress_cfg_.ref_level = std::max(stress_cfg_.max_level + 1, stress_cfg_.ref_level);

    ImGui::SetNextItemWidth(140);
    ImGui::InputInt("sample points", &stress_cfg_.sample_points);
    stress_cfg_.sample_points = std::max(0, stress_cfg_.sample_points);

    int seed_i = (int)stress_cfg_.seed;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("seed", &seed_i);
    seed_i = std::max(0, seed_i);
    stress_cfg_.seed = (unsigned int)seed_i;

    ImGui::Checkbox("point-wise", &stress_cfg_.include_pointwise);
    ImGui::SameLine();
    ImGui::Checkbox("global L2 vs ref", &stress_cfg_.include_global_l2);

    ImGui::Spacing();

    if (ImGui::Button("Run + Export CSV")) {
        stress_status_message_.clear();

        LOGT_INFO(LogMath,
                  "Aitken stress: run+export (levels %d..%d, ref=%d, points=%d, pointwise=%d, global_l2=%d)",
                  stress_cfg_.min_level,
                  stress_cfg_.max_level,
                  stress_cfg_.ref_level,
                  stress_cfg_.sample_points,
                  stress_cfg_.include_pointwise ? 1 : 0,
                  stress_cfg_.include_global_l2 ? 1 : 0);

        struct LevelSol {
            int level = 0;
            double h = 0.0;
            FEMMesh mesh;
            std::vector<double> u;
        };

        std::vector<LevelSol> levels;
        levels.reserve((size_t)(stress_cfg_.max_level - stress_cfg_.min_level + 1));

        for (int lvl = stress_cfg_.min_level; lvl <= stress_cfg_.max_level; ++lvl) {
            ReferenceSolveRequest req;
            req.refinement_level = lvl;
            ReferenceSolution out;
            if (!rp->solve_reference(req, out) || !out.sol.is_ready()) {
                stress_status_message_ = "Solve failed at level " + std::to_string(lvl);
                break;
            }

            LevelSol L;
            L.level = lvl;
            L.mesh = std::move(out.mesh);
            L.u = std::move(out.sol.solution_u);
            L.h = mesh_h_max_edge<double>(L.mesh);
            levels.push_back(std::move(L));
        }

        if (stress_status_message_.empty()) {
            LOGT_INFO(LogMath, "Aitken stress: solved %zu levels", levels.size());
        }

        FEMMesh ref_mesh;
        std::vector<double> ref_u;
        TriLocator ref_loc;
        if (stress_status_message_.empty() && stress_cfg_.include_global_l2) {
            LOGT_INFO(LogMath, "Aitken stress: solving reference level %d", stress_cfg_.ref_level);
            ReferenceSolveRequest req;
            req.refinement_level = stress_cfg_.ref_level;
            ReferenceSolution out;
            if (!rp->solve_reference(req, out) || !out.sol.is_ready()) {
                stress_status_message_ = "Reference solve failed at level " + std::to_string(stress_cfg_.ref_level);
            } else {
                ref_mesh = std::move(out.mesh);
                ref_u = std::move(out.sol.solution_u);
                ref_loc.build(ref_mesh, 64);
                LOGT_INFO(LogMath, "Aitken stress: reference ready (dofs=%d)", ref_mesh.dof_count());
            }
        }

        if (stress_status_message_.empty() && levels.size() < 3) {
            stress_status_message_ = "Need at least 3 levels";
        }

        const std::string prefix = export_prefix_to_pathsafe_prefix(export_path_);
        const std::string path_points = prefix + "_aitken_points.csv";
        const std::string path_global = prefix + "_aitken_global.csv";

        bool wrote_any = false;

        if (stress_status_message_.empty() && stress_cfg_.include_pointwise) {
            LOGT_INFO(LogMath, "Aitken stress: exporting pointwise CSV to %s", path_points.c_str());

            // Build locators only if needed (and keep them bounded to a smaller grid).
            std::vector<TriLocator> locators;
            locators.resize(levels.size());
            for (std::size_t i = 0; i < levels.size(); ++i) {
                LOGT_INFO(LogMath, "Aitken stress: building locator for level %d", levels[i].level);
                locators[i].build(levels[i].mesh, 64);
            }

            std::vector<std::pair<double,double>> pts;
            if (!sample_points_in_mesh(levels.back().mesh, stress_cfg_.sample_points, stress_cfg_.seed, pts)) {
                stress_status_message_ = "Failed to sample points";
            } else {
                std::ofstream f(path_points, std::ios::binary);
                if (!f) {
                    stress_status_message_ = "Failed to write " + path_points;
                } else {
                    f.setf(std::ios::scientific);
                    f.precision(16);
                    f << "point_id,x,y,level1,level2,level3,h1,h2,h3,q1,q2,q3,valid,p_est,q_inf_est,err_fine_est\n";

                    std::size_t pid = 0;
                    for (const auto& p : pts) {
                        const double x = p.first;
                        const double y = p.second;
                        for (std::size_t i = 0; i + 2 < levels.size(); ++i) {
                            const auto& L1 = levels[i];
                            const auto& L2 = levels[i + 1];
                            const auto& L3 = levels[i + 2];

                            double q1 = 0.0, q2 = 0.0, q3 = 0.0;
                            const bool ok1 = eval_p1_at<double>(L1.mesh, L1.u, x, y, q1, nullptr, &locators[i]);
                            const bool ok2 = eval_p1_at<double>(L2.mesh, L2.u, x, y, q2, nullptr, &locators[i + 1]);
                            const bool ok3 = eval_p1_at<double>(L3.mesh, L3.u, x, y, q3, nullptr, &locators[i + 2]);

                            bool valid = false;
                            double p_est = 0.0, q_inf = 0.0, err_est = 0.0;
                            if (ok1 && ok2 && ok3) {
                                auto est = aitken_estimate_3<double>(q1, q2, q3, L1.h, L2.h, L3.h);
                                valid = est.valid;
                                p_est = est.p;
                                q_inf = est.q_inf;
                                err_est = est.err_fine;
                            }

                            f << pid << "," << x << "," << y << ","
                              << L1.level << "," << L2.level << "," << L3.level << ","
                              << L1.h << "," << L2.h << "," << L3.h << ","
                              << q1 << "," << q2 << "," << q3 << ","
                              << (valid ? 1 : 0) << "," << p_est << "," << q_inf << "," << err_est << "\n";
                        }
                        ++pid;

                        if ((pid % 200) == 0) {
                            LOGT_INFO(LogMath, "Aitken stress: pointwise progress %zu/%zu", pid, pts.size());
                        }
                    }

                    f.flush();
                    if (!f.good()) {
                        stress_status_message_ = "Failed to write " + path_points;
                    } else {
                        wrote_any = true;
                    }
                }
            }
        }

        if (stress_status_message_.empty() && stress_cfg_.include_global_l2) {
            LOGT_INFO(LogMath, "Aitken stress: exporting global CSV to %s", path_global.c_str());

            std::ofstream f(path_global, std::ios::binary);
            if (!f) {
                stress_status_message_ = "Failed to write " + path_global;
            } else {
                f.setf(std::ios::scientific);
                f.precision(16);
                f << "level,h,l2_vs_ref,rate_l2,aitken_p,aitken_err_est\n";

                std::vector<double> l2;
                l2.reserve(levels.size());

                for (const auto& L : levels) {
                    auto e = compute_error_vs_reference<double>(L.mesh, L.u, ref_mesh, ref_u, &ref_loc);
                    l2.push_back(e.valid ? e.l2 : std::numeric_limits<double>::quiet_NaN());
                }

                LOGT_INFO(LogMath, "Aitken stress: computed L2 vs ref for %zu levels", levels.size());

                for (std::size_t i = 0; i < levels.size(); ++i) {
                    const auto& L = levels[i];
                    const double li = l2[i];

                    double rate = std::numeric_limits<double>::quiet_NaN();
                    if (i > 0 && std::isfinite(l2[i - 1]) && std::isfinite(li) && l2[i - 1] > 0.0 && li > 0.0) {
                        const double log_h = std::log(levels[i - 1].h / std::max(L.h, 1e-16));
                        if (std::abs(log_h) > 1e-12) rate = std::log(l2[i - 1] / li) / log_h;
                    }

                    double ap = std::numeric_limits<double>::quiet_NaN();
                    double ae = std::numeric_limits<double>::quiet_NaN();
                    if (i >= 2 && std::isfinite(l2[i - 2]) && std::isfinite(l2[i - 1]) && std::isfinite(li)) {
                        auto est = aitken_estimate_3<double>(l2[i - 2], l2[i - 1], li, levels[i - 2].h, levels[i - 1].h, L.h);
                        if (est.valid) {
                            ap = est.p;
                            ae = est.err_fine;
                        }
                    }

                    f << L.level << "," << L.h << "," << li << "," << rate << "," << ap << "," << ae << "\n";
                }

                f.flush();
                if (!f.good()) {
                    stress_status_message_ = "Failed to write " + path_global;
                } else {
                    wrote_any = true;
                }
            }
        }

        if (stress_status_message_.empty()) {
            if (wrote_any) {
                stress_status_message_ = "✓ Exported: " + path_points + " and/or " + path_global;
            } else {
                stress_status_message_ = "Nothing to export";
            }
        }
    }

    if (!stress_status_message_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", stress_status_message_.c_str());
    }
}

void FEMErrorAnalysisWindow::draw_section_export_(const DrawInfo& info) {
    ImGui::SeparatorText("Export Settings");
    
    char buf[256];
    std::strncpy(buf, export_path_.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    if (ImGui::InputText("Output prefix", buf, sizeof(buf))) {
        export_path_ = buf;
    }
    
    ImGui::TextDisabled("Files: prefix_convergence.csv, prefix_table.tex, etc.");
}

void FEMErrorAnalysisWindow::draw_section_flux_analysis_(const DrawInfo& info) {
    ImGui::SeparatorText("Flux & Balance Analysis");

    ImGui::InputDouble("outer tol##flux", &balance_cfg_.outer_classify_tol, 0.0, 0.0, "%.6f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Tolerance for classifying edges as left/right/bottom/top by bbox.");

    if (ImGui::Button("Compute Flux Balance")) {
        FEMProblem prob;
        if (info.pde) info.pde->fill_fem_problem(prob);
        prob.mesh = &cached_fem_;

        balance_ = compute_balance_metrics(
            cached_fem_, info.sol->solution_u,
            prob.a, prob.c, prob.f,
            balance_cfg_
        );
        has_balance_ = true;
    }

    if (!has_balance_) return;

    ImGui::Spacing();
    ImGui::Separator();

    auto color_flux = [](double v) {
        return (v >= 0.0) ? ImVec4(0.4f,1.0f,0.4f,1) : ImVec4(1.0f,0.7f,0.3f,1);
    };

    ImGui::Text("Domain Area: %.6g", balance_.domain_area);
    ImGui::Text("u range: [%.6g, %.6g]", balance_.u_min, balance_.u_max);

    ImGui::Spacing();
    if (ImGui::BeginTable("FluxTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Flux", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto row = [&](const char* label, double val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", label);
            ImGui::TableNextColumn(); ImGui::TextColored(color_flux(val), "%.6e", val);
        };

        row("Left (Q_left)", balance_.flux_left);
        row("Right (Q_right)", balance_.flux_right);
        row("Bottom (Q_bottom)", balance_.flux_bottom);
        row("Top (Q_top)", balance_.flux_top);
        row("Other outer", balance_.flux_outer);
        row("Inner pore walls", balance_.inner_exchange);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Text("Inner perimeter: %.6g", balance_.inner_perimeter);
    ImGui::Text("Inner flux / perimeter: %.6e", balance_.inner_exchange_per_length);
    ImGui::Text("k_eff (Q_right / (du/W)): %.6e", balance_.k_eff);

    ImGui::Spacing();
    ImGui::Text("int(f) dOmega: %.6e", balance_.integral_f);
    ImGui::Text("int(c*u) dOmega: %.6e", balance_.integral_cu);

    const double residual = balance_.conservation_residual;
    ImVec4 res_col = (std::abs(residual) < 1e-6)
        ? ImVec4(0.4f,1.0f,0.4f,1.0f)
        : ImVec4(1.0f,0.3f,0.3f,1.0f);
    ImGui::TextColored(res_col, "Conservation residual: %.6e", residual);

    ImGui::Spacing();
    if (ImGui::Button("Copy CSV##flux")) {
        std::string csv = balance_metrics_csv_header() + "\n" + balance_metrics_csv_row(balance_) + "\n";
        ImGui::SetClipboardText(csv.c_str());
    }
}

void FEMErrorAnalysisWindow::draw_section_self_tests_(const DrawInfo& info) {
    ImGui::SeparatorText("Self-Tests (Validation)");

    if (ImGui::CollapsingHeader("Robin Slab (machine precision)")) {
        ImGui::TextWrapped(
            "-kappa * Lapl(u) = 0 on [0,2]x[0,1], u(0)=1 Dirichlet, "
            "du/dn + 5u = 7 Robin at x=2. "
            "P1 FEM must reproduce exact linear solution.\n");

        if (ImGui::Button("Run Robin Slab Test")) {
            robin_result_ = run_robin_slab_self_test();
            self_test_robin_ran_ = true;
        }
        if (self_test_robin_ran_) {
            if (robin_result_.passed) {
                ImGui::TextColored(ImVec4(0.2f,1,0.2f,1), "PASSED");
            } else {
                ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "FAILED");
            }
            ImGui::Text("max|u_h - u_ex| = %.3e", robin_result_.max_abs_err);
            ImGui::Text("Expected slope A = %.12g", robin_result_.expected_exact_slope);
            ImGui::Text("Got slope      A = %.12g", robin_result_.got_slope);
        }
    }

    if (ImGui::CollapsingHeader("MMS Convergence (sin πx sin πy)")) {
        ImGui::TextWrapped(
            "-kappa * Lapl(u) = f on [0,1]^2, u=0 on dOmega, "
            "u_exact = sin(pi*x)sin(pi*y). "
            "Expected: rate_L2 ~ 2, rate_H1 ~ 1 for P1.\n");

        ImGui::SliderFloat("kappa##mms", &mms_kappa_, 0.01f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

        if (ImGui::Button("Run MMS Study")) {
            mms_result_ = run_mms_convergence_study((double)mms_kappa_);
            self_test_mms_ran_ = true;
        }
        if (self_test_mms_ran_) {
            if (mms_result_.passed) {
                ImGui::TextColored(ImVec4(0.2f,1,0.2f,1), "PASSED");
            } else {
                ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "FAILED");
            }
            ImGui::Text("Avg rate L2: %.3f  (expected ~ 2)", mms_result_.avg_rate_l2);
            ImGui::Text("Avg rate H1: %.3f  (expected ~ 1)", mms_result_.avg_rate_h1);

            if (!mms_result_.levels.empty() &&
                ImGui::BeginTable("MMSTable", 7,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                    ImVec2(0, 200)))
            {
                ImGui::TableSetupColumn("N");
                ImGui::TableSetupColumn("DOFs");
                ImGui::TableSetupColumn("h");
                ImGui::TableSetupColumn("L2");
                ImGui::TableSetupColumn("rate L2");
                ImGui::TableSetupColumn("H1");
                ImGui::TableSetupColumn("rate H1");
                ImGui::TableHeadersRow();

                for (const auto& L : mms_result_.levels) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%d", L.n);
                    ImGui::TableNextColumn(); ImGui::Text("%d", L.dofs);
                    ImGui::TableNextColumn(); ImGui::Text("%.3e", L.h);
                    ImGui::TableNextColumn(); ImGui::Text("%.3e", L.l2);
                    ImGui::TableNextColumn();
                    if (L.rate_l2 > 0) ImGui::Text("%.2f", L.rate_l2);
                    else ImGui::TextDisabled("-");
                    ImGui::TableNextColumn(); ImGui::Text("%.3e", L.h1);
                    ImGui::TableNextColumn();
                    if (L.rate_h1 > 0) ImGui::Text("%.2f", L.rate_h1);
                    else ImGui::TextDisabled("-");
                }
                ImGui::EndTable();
            }

            if (ImGui::Button("Copy CSV##mms")) {
                ImGui::SetClipboardText(mms_result_.csv.c_str());
            }
        }
    }
}

void FEMErrorAnalysisWindow::draw_section_fractional_comparison_(const DrawInfo& info) {
    ImGui::SeparatorText("Fractional vs Classical Comparison");
    ImGui::TextWrapped(
        "Solves the same BVP with classical -Delta u = f and each selected "
        "fractional operator on the current mesh. Runs in a background thread.\n"
    );

    ImGui::SliderFloat("s (frac. order)##frac_cmp", &frac_cmp_.s, 0.05f, 0.95f, "%.2f");

    ImGui::Checkbox("Spectral##frac_type", &frac_cmp_.use_spectral);
    ImGui::SameLine();
    ImGui::Checkbox("Integral##frac_type", &frac_cmp_.use_integral);
    ImGui::SameLine();
    ImGui::Checkbox("Regional##frac_type", &frac_cmp_.use_regional);

    const bool is_running = frac_cmp_.running.load();

    if (is_running) {
        const int prog = frac_cmp_.progress.load();
        ImGui::ProgressBar((float)prog / (float)FracCompare::NUM_SLOTS, ImVec2(-1, 0),
            "Computing...");
    }

    if (!is_running && frac_cmp_.worker.joinable()) {
        frac_cmp_.join();
        frac_cmp_.ran = true;
    }

    {
        const bool can_run = !is_running &&
            (frac_cmp_.use_spectral || frac_cmp_.use_integral || frac_cmp_.use_regional);

        if (!can_run) ImGui::BeginDisabled();
        if (ImGui::Button("Run Comparison")) {
            frac_cmp_.join();  // join any previous
            frac_cmp_.ran = false;
            frac_cmp_.progress.store(0);

            const FEMMesh mesh_copy = cached_fem_;
            FEMProblem prob_base;
            if (info.pde) info.pde->fill_fem_problem(prob_base);

            const float   s_val        = frac_cmp_.s;
            const bool    do_spectral  = frac_cmp_.use_spectral;
            const bool    do_integral  = frac_cmp_.use_integral;
            const bool    do_regional  = frac_cmp_.use_regional;

            ExactSolution exact_snap;
            const bool has_exact = try_fill_exact_(exact_snap, info.pde);

            frac_cmp_.running.store(true);

            frac_cmp_.worker = std::thread([this,
                mesh_copy, prob_base, s_val,
                do_spectral, do_integral, do_regional,
                has_exact, exact_snap]() mutable
            {
                auto& cmp = frac_cmp_;

                // Set mesh pointer
                FEMMesh local_mesh = std::move(mesh_copy);
                FEMProblem pb = prob_base;
                pb.mesh = &local_mesh;

                const double W = mesh_bbox_width(local_mesh);

                // BC diagnostics
                cmp.n_robin_edges = 0;
                cmp.robin_perimeter = 0.0;
                cmp.n_dirichlet_edges = 0;
                cmp.n_neumann_edges = 0;
                double sum_k = 0.0, sum_g = 0.0;
                for (const auto& e : local_mesh.edges_bc) {
                    if (e.type == BCType::Robin) {
                        cmp.n_robin_edges++;
                        double L = std::hypot(
                            local_mesh.nodes[e.b].x - local_mesh.nodes[e.a].x,
                            local_mesh.nodes[e.b].y - local_mesh.nodes[e.a].y);
                        cmp.robin_perimeter += L;
                        sum_k += e.k; sum_g += e.g;
                    } else if (e.type == BCType::Dirichlet) {
                        cmp.n_dirichlet_edges++;
                    } else if (e.type == BCType::Neumann) {
                        cmp.n_neumann_edges++;
                    }
                }
                if (cmp.n_robin_edges > 0) {
                    cmp.robin_avg_k = sum_k / cmp.n_robin_edges;
                    cmp.robin_avg_g = sum_g / cmp.n_robin_edges;
                }
                cmp.robin_k_is_zero = (cmp.n_robin_edges > 0 &&
                                       std::abs(cmp.robin_avg_k) < 1e-12);

                // Helper: compute Robin-boundary average u
                auto robin_avg_u = [&](std::span<const double> u) -> double {
                    double wsum = 0.0, wlen = 0.0;
                    for (const auto& e : local_mesh.edges_bc) {
                        if (e.type != BCType::Robin) continue;
                        double L = std::hypot(
                            local_mesh.nodes[e.b].x - local_mesh.nodes[e.a].x,
                            local_mesh.nodes[e.b].y - local_mesh.nodes[e.a].y);
                        wsum += L * 0.5 * (u[e.a] + u[e.b]);
                        wlen += L;
                    }
                    return (wlen > 0) ? wsum / wlen : 0.0;
                };

                auto fill_slot = [&](FracCompare::SlotResult& slot,
                                     FEMProblem& prob,
                                     bool is_classical,
                                     const char* branch_label)
                {
                    // Log Robin parameters that will enter this branch's assembly
                    {
                        int n_rob = 0;
                        double sum_k = 0, sum_g = 0;
                        for (const auto& e : local_mesh.edges_bc) {
                            if (e.type == BCType::Robin) {
                                ++n_rob;
                                sum_k += e.k;
                                sum_g += e.g;
                            }
                        }
                        if (n_rob > 0) {
                            LOGT_INFO(LogMath,
                                "[FracCompare] %s: %d Robin edges, avg k=%.6g, avg g=%.6g",
                                branch_label, n_rob, sum_k / n_rob, sum_g / n_rob);
                        } else {
                            LOGT_INFO(LogMath, "[FracCompare] %s: no Robin edges", branch_label);
                        }
                    }

                    DifferentialEquationSolution sol;
                    if (is_classical) {
                        assemble_and_solve_local_P1(prob, sol);
                    } else {
                        assemble_and_solve_fractional_auto_P1(prob, sol);
                    }
                    slot.u = sol.solution_u;
                    slot.energy = compute_energy_terms(
                        local_mesh, sol.solution_u, prob.c, prob.f);

                    if (is_classical) {
                        slot.energy.bilinear_energy = compute_classical_bilinear_energy(
                            local_mesh, sol.solution_u, prob.a);
                    } else if (prob.fractional &&
                               prob.fractional->type == FractionalType::Spectral) {
                        // Spectral: energy computed inside the spectral solver
                        slot.energy.bilinear_energy = sol.spectral_bilinear_energy;
                    } else {
                        // Integral / Regional: compute from the kernel formula directly
                        const double s_loc = prob.fractional ? (double)prob.fractional->s : 0.5;
                        const double C_loc = prob.fractional ? (double)prob.fractional->scale : 1.0;
                        slot.energy.bilinear_energy = compute_fractional_bilinear_energy(
                            local_mesh, sol.solution_u, s_loc, C_loc);
                    }

                    // Dirichlet boundary work — operator-consistent computation
                    if (is_classical || (prob.fractional &&
                        prob.fractional->type == FractionalType::Spectral)) {
                        // Classical / Spectral: gradient-flux-based
                        slot.energy.dirichlet_work = compute_dirichlet_boundary_work(
                            local_mesh, sol.solution_u, prob.a);
                    } else {
                        // Integral / Regional: nonlocal interaction work
                        const double s_loc = prob.fractional ? (double)prob.fractional->s : 0.5;
                        const double C_loc = prob.fractional ? (double)prob.fractional->scale : 1.0;
                        slot.energy.dirichlet_work = compute_nonlocal_dirichlet_work(
                            local_mesh, sol.solution_u, s_loc, C_loc);
                    }

                    finalize_energy_metrics(slot.energy, W);
                    slot.inner_avg_u = robin_avg_u(sol.solution_u);

                    LOGT_INFO(LogMath,
                        "[FracCompare] %s result: bilinear=%.6g, robin_energy=%.6g, "
                        "robin_exchange=%.6g, dirichlet_work=%.6g, residual=%.6g, "
                        "u_min=%.6g, u_max=%.6g, bbox_W=%.6g",
                        branch_label,
                        slot.energy.bilinear_energy,
                        slot.energy.robin_energy,
                        slot.energy.robin_exchange,
                        slot.energy.dirichlet_work,
                        slot.energy.energy_residual,
                        slot.energy.u_min,
                        slot.energy.u_max,
                        W);

                    if (has_exact) {
                        ExactSolution ex = exact_snap;
                        slot.err = compute_error_metrics<double>(
                            local_mesh, sol.solution_u, &ex);
                    }
                    slot.active = true;
                };

                for (auto& sl : cmp.slots) { sl.active = false; sl.u.clear(); }

                {
                    FEMProblem prob_c = pb;
                    prob_c.a = 1.0;
                    prob_c.fractional.reset();
                    fill_slot(cmp.slots[FracCompare::SLOT_CLASSICAL], prob_c, true, "Classical");
                    cmp.progress.store(1);
                }

                if (do_spectral) {
                    FEMProblem prob_f = pb;
                    prob_f.a = 1.0;
                    prob_f.fractional = FractionalEquationConfig{};
                    prob_f.fractional->s = (double)s_val;
                    prob_f.fractional->scale = 1.0;
                    prob_f.fractional->type = FractionalType::Spectral;
                    prob_f.fractional->spectral_k = std::min(200, local_mesh.dof_count());
                    fill_slot(cmp.slots[FracCompare::SLOT_SPECTRAL], prob_f, false, "Spectral");
                }
                cmp.progress.store(2);

                if (do_integral) {
                    FEMProblem prob_f = pb;
                    prob_f.a = 1.0;
                    prob_f.fractional = FractionalEquationConfig{};
                    prob_f.fractional->s = (double)s_val;
                    prob_f.fractional->scale = 1.0;
                    prob_f.fractional->type = FractionalType::Integral;
                    fill_slot(cmp.slots[FracCompare::SLOT_INTEGRAL], prob_f, false, "Integral");
                }
                cmp.progress.store(3);

                if (do_regional) {
                    FEMProblem prob_f = pb;
                    prob_f.a = 1.0;
                    prob_f.fractional = FractionalEquationConfig{};
                    prob_f.fractional->s = (double)s_val;
                    prob_f.fractional->scale = 1.0;
                    prob_f.fractional->type = FractionalType::Regional;
                    fill_slot(cmp.slots[FracCompare::SLOT_REGIONAL], prob_f, false, "Regional");
                }
                cmp.progress.store(4);

                cmp.running.store(false);
            });
        }
        if (!can_run) ImGui::EndDisabled();
    }

    if (!frac_cmp_.ran) return;

    if (frac_cmp_.robin_k_is_zero) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1,0.4f,0,1),
            "WARNING: Robin k ~ 0 on %d edges (avg k=%.4g, avg g=%.4g).",
            frac_cmp_.n_robin_edges, frac_cmp_.robin_avg_k, frac_cmp_.robin_avg_g);
        ImGui::TextColored(ImVec4(1,0.4f,0,1),
            "This degenerates Robin to Neumann (du/dn = g). "
            "For a penalty-type Robin sink, set Value > 0 in the BC.");
    }

    ImGui::Spacing();
    ImGui::Text("BC summary: %d Dirichlet, %d Robin (perim=%.2f), %d Neumann edges",
        frac_cmp_.n_dirichlet_edges, frac_cmp_.n_robin_edges,
        frac_cmp_.robin_perimeter, frac_cmp_.n_neumann_edges);
    if (frac_cmp_.n_robin_edges > 0) {
        ImGui::Text("Robin avg k=%.4g, avg g=%.4g",
            frac_cmp_.robin_avg_k, frac_cmp_.robin_avg_g);
    }

    // Count active columns
    int n_active = 0;
    for (const auto& sl : frac_cmp_.slots) if (sl.active) n_active++;
    if (n_active == 0) return;

    // Robin avg u for active slots
    if (frac_cmp_.n_robin_edges > 0) {
        ImGui::Text("Inner-boundary <u>:");
        for (const auto& sl : frac_cmp_.slots) {
            if (!sl.active) continue;
            ImGui::SameLine();
            ImGui::Text("%s=%.4e", sl.label, sl.inner_avg_u);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Dynamic table: Metric | active columns...
    if (ImGui::BeginTable("FracCmpTable", 1 + n_active,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 220);
        for (const auto& sl : frac_cmp_.slots) {
            if (!sl.active) continue;
            char col_label[64];
            if (&sl == &frac_cmp_.slots[FracCompare::SLOT_CLASSICAL])
                snprintf(col_label, sizeof(col_label), "%s", sl.label);
            else
                snprintf(col_label, sizeof(col_label), "%s=%.2f", sl.label, frac_cmp_.s);
            ImGui::TableSetupColumn(col_label, ImGuiTableColumnFlags_WidthStretch);
        }
        ImGui::TableHeadersRow();

        auto row = [&](const char* label, auto getter) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", label);
            for (const auto& sl : frac_cmp_.slots) {
                if (!sl.active) continue;
                ImGui::TableNextColumn();
                ImGui::Text("%.6e", getter(sl));
            }
        };

        row("Bilinear energy a(u,u)/2",  [](const auto& s) { return s.energy.bilinear_energy; });
        row("Robin bdry energy k*u^2/2", [](const auto& s) { return s.energy.robin_energy; });
        row("Reaction energy c*u^2/2",   [](const auto& s) { return s.energy.reaction_energy; });
        row("Total internal",            [](const auto& s) { return s.energy.total_internal; });

        // Separator row
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextDisabled("---");
        for (const auto& sl : frac_cmp_.slots) {
            if (!sl.active) continue;
            ImGui::TableNextColumn(); ImGui::TextDisabled("---");
        }

        row("Source work int(f*u)",       [](const auto& s) { return s.energy.source_work; });
        row("Robin work int(g*u)",        [](const auto& s) { return s.energy.robin_work; });
        row("Neumann work int(gN*u)",     [](const auto& s) { return s.energy.neumann_work; });
        // Dirichlet work: only meaningful for classical (gradient-based)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Dirichlet bdry work");
            for (int si = 0; si < FracCompare::NUM_SLOTS; ++si) {
                const auto& sl = frac_cmp_.slots[si];
                if (!sl.active) continue;
                ImGui::TableNextColumn();
                if (si == FracCompare::SLOT_CLASSICAL)
                    ImGui::Text("%.6e", sl.energy.dirichlet_work);
                else
                    ImGui::TextDisabled("(nonlocal)");
            }
        }
        // Total external: only physically meaningful where Dirichlet work is well-defined
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Total external");
            for (int si = 0; si < FracCompare::NUM_SLOTS; ++si) {
                const auto& sl = frac_cmp_.slots[si];
                if (!sl.active) continue;
                ImGui::TableNextColumn();
                if (si == FracCompare::SLOT_CLASSICAL)
                    ImGui::Text("%.6e", sl.energy.total_external);
                else
                    ImGui::TextDisabled("N/A");
            }
        }

        // Separator row
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextDisabled("---");
        for (const auto& sl : frac_cmp_.slots) {
            if (!sl.active) continue;
            ImGui::TableNextColumn(); ImGui::TextDisabled("---");
        }

        // Energy balance residual: only valid for classical operator
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Energy balance residual");
            for (int si = 0; si < FracCompare::NUM_SLOTS; ++si) {
                const auto& sl = frac_cmp_.slots[si];
                if (!sl.active) continue;
                ImGui::TableNextColumn();
                if (si == FracCompare::SLOT_CLASSICAL) {
                    const double rel = (sl.energy.total_internal > 1e-12)
                        ? std::abs(sl.energy.energy_residual) / sl.energy.total_internal
                        : std::abs(sl.energy.energy_residual);
                    const auto col = (rel < 1e-4)
                        ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                        : (rel < 1e-1)
                            ? ImVec4(1.0f, 1.0f, 0.4f, 1.0f)
                            : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    ImGui::TextColored(col, "%.6e", sl.energy.energy_residual);
                } else {
                    ImGui::TextDisabled("N/A (nonlocal)");
                }
            }
        }

        // Separator row
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextDisabled("---");
        for (const auto& sl : frac_cmp_.slots) {
            if (!sl.active) continue;
            ImGui::TableNextColumn(); ImGui::TextDisabled("---");
        }

        row("Robin exchange (ku-g)",      [](const auto& s) { return s.energy.robin_exchange; });
        row("Robin exch./perimeter",      [](const auto& s) { return s.energy.robin_exchange_per_length; });
        row("int(f) dOmega",             [](const auto& s) { return s.energy.integral_f; });
        row("u min",                      [](const auto& s) { return s.energy.u_min; });
        row("u max",                      [](const auto& s) { return s.energy.u_max; });

        ImGui::EndTable();
    }

    // Robin-exchange ratio: the primary comparable metric
    const auto& C = frac_cmp_.slots[FracCompare::SLOT_CLASSICAL];
    if (C.active && std::abs(C.energy.robin_exchange) > 1e-12) {
        ImGui::Spacing();
        for (int i = 1; i < FracCompare::NUM_SLOTS; ++i) {
            const auto& sl = frac_cmp_.slots[i];
            if (!sl.active) continue;
            const double ratio = sl.energy.robin_exchange / C.energy.robin_exchange;
            ImGui::Text("Robin-exchange ratio (%s / classical): %.4f", sl.label, ratio);
            if (ratio > 1.0) {
                ImGui::TextColored(ImVec4(0.4f,1,0.4f,1),
                    "  %s pore-wall uptake is %.1f%% higher",
                    sl.label, (ratio - 1.0) * 100.0);
            } else if (ratio < 1.0) {
                ImGui::TextColored(ImVec4(1,1,0.4f,1),
                    "  %s pore-wall uptake is %.1f%% lower",
                    sl.label, (1.0 - ratio) * 100.0);
            }
        }
        // Bilinear energy ratio (operator-dependent, for reference)
        if (C.energy.bilinear_energy > 1e-12) {
            for (int i = 1; i < FracCompare::NUM_SLOTS; ++i) {
                const auto& sl = frac_cmp_.slots[i];
                if (!sl.active) continue;
                const double ratio = sl.energy.bilinear_energy / C.energy.bilinear_energy;
                ImGui::TextDisabled("Bilinear energy ratio (%s / classical): %.4f (op-dependent)",
                    sl.label, ratio);
            }
        }
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Energy balance residual is shown only for the classical operator "
        "(gradient-flux Dirichlet work). For nonlocal operators, the correct "
        "energy identity requires Omega x Omega^c interaction splitting which "
        "is not implemented. Robin exchange = int(k*u - g) ds is the primary "
        "comparable metric across all operator types."
    );

    // Error norms table (only if exact solution available)
    bool any_has_exact = false;
    for (const auto& sl : frac_cmp_.slots)
        if (sl.active && sl.err.has_exact) { any_has_exact = true; break; }

    if (any_has_exact) {
        ImGui::Spacing();
        if (ImGui::BeginTable("FracErrTable", 1 + n_active,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Norm");
            for (const auto& sl : frac_cmp_.slots) {
                if (!sl.active) continue;
                ImGui::TableSetupColumn(sl.label);
            }
            ImGui::TableHeadersRow();

            auto erow = [&](const char* label, auto getter) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", label);
                for (const auto& sl : frac_cmp_.slots) {
                    if (!sl.active) continue;
                    ImGui::TableNextColumn();
                    if (sl.err.has_exact) ImGui::Text("%.4e", getter(sl));
                    else ImGui::TextDisabled("-");
                }
            };

            erow("Linf nodal", [](const auto& s) { return s.err.linf_nodes; });
            erow("L2",         [](const auto& s) { return s.err.l2; });

            bool any_grad = false;
            for (const auto& sl : frac_cmp_.slots)
                if (sl.active && sl.err.has_grad) { any_grad = true; break; }
            if (any_grad)
                erow("H1 semi", [](const auto& s) { return s.err.h1_semi; });

            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Copy Comparison CSV##frac")) {
        std::ostringstream csv;
        csv << std::scientific << std::setprecision(8);
        csv << "metric";
        for (const auto& sl : frac_cmp_.slots) {
            if (!sl.active) continue;
            csv << "," << sl.label;
        }
        csv << "\n";

        auto csv_row = [&](const char* label, auto getter) {
            csv << label;
            for (const auto& sl : frac_cmp_.slots) {
                if (!sl.active) continue;
                csv << "," << getter(sl);
            }
            csv << "\n";
        };

        csv_row("bilinear_energy",  [](const auto& s) { return s.energy.bilinear_energy; });
        csv_row("robin_energy",     [](const auto& s) { return s.energy.robin_energy; });
        csv_row("reaction_energy",  [](const auto& s) { return s.energy.reaction_energy; });
        csv_row("total_internal",   [](const auto& s) { return s.energy.total_internal; });
        csv_row("source_work",      [](const auto& s) { return s.energy.source_work; });
        csv_row("robin_work",       [](const auto& s) { return s.energy.robin_work; });
        csv_row("neumann_work",     [](const auto& s) { return s.energy.neumann_work; });
        csv_row("robin_exchange",   [](const auto& s) { return s.energy.robin_exchange; });
        csv_row("robin_exch_per_L", [](const auto& s) { return s.energy.robin_exchange_per_length; });
        csv_row("u_min",            [](const auto& s) { return s.energy.u_min; });
        csv_row("u_max",            [](const auto& s) { return s.energy.u_max; });

        ImGui::SetClipboardText(csv.str().c_str());
    }
}

void FEMErrorAnalysisWindow::draw(const DrawInfo& info) {
    if (!visible) return;

    ImGui::Begin("FEM Error Analysis", &visible, 
                 ImGuiWindowFlags_AlwaysVerticalScrollbar);

    if (!info.mesh || !info.pde || !info.sol || !info.sol->is_ready()) {
        ImGui::TextDisabled("No mesh/PDE/solution available.");
        ImGui::TextWrapped("Solve a PDE problem first to enable error analysis.");
        ImGui::End();
        return;
    }

    ensure_cache_(*info.mesh);
    
    draw_section_mesh_info_(info);
    ImGui::Spacing();
    
    draw_section_local_analysis_(info);
    ImGui::Spacing();
    
    draw_section_global_norms_(info);
    ImGui::Spacing();
    
    draw_section_flux_analysis_(info);
    ImGui::Spacing();

    draw_section_convergence_study_(info);
    ImGui::Spacing();
    
    draw_section_aitken_analysis_(info);
    ImGui::Spacing();

    draw_section_aitken_stress_(info);
    ImGui::Spacing();

    draw_section_self_tests_(info);
    ImGui::Spacing();

    draw_section_fractional_comparison_(info);
    ImGui::Spacing();
    
    draw_section_export_(info);

    ImGui::End();
}

} // namespace fem
