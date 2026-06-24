#include "pde_component.h"
#include "pde_preset.h"
#include "pde_presets.h"
#include "math/fem/fem_simulation.h"
#include "log_categories.h"
#include "geom/mesh_component.h"
#include "math/entities/planar_math_entity.h"
#include "core/entity/entity.h"
#include "math/fem/dirichlet_map.h"
#include "math/fem/fem_solve_dispatcher.h"
#include "math/pde/pde_solve_request_builder.h"
#include "math/fem/fem_boundary_adapter.h"
#include <limits>
#include <algorithm>

namespace fem {

FEM_DEFINE_OBJECT(PDEComponent, Component, DisplayName("Partial Differential Equation"), DrawCallbacks());
FEM_BEGIN_PROPERTY_REGISTER(PDEComponent)
{
    FEM_REGISTER_PROPERTY(PDEComponent, equation_preset_, DisplayName("PDE Preset"), BaseClass(), NoTypeHeader());
    FEM_REGISTER_PROPERTY(PDEComponent, solution_method_);

    FEM_REGISTER_PROPERTY(PDEComponent, time_playback_enabled_, DisplayName("Time Playback"), NoUI());
    FEM_REGISTER_PROPERTY(PDEComponent, time_playing_, DisplayName("Play"), NoUI());
    FEM_REGISTER_PROPERTY(PDEComponent, time_seconds_, DisplayName("t (seconds)"), NoUI());
    FEM_REGISTER_PROPERTY(PDEComponent, time_step_seconds_, DisplayName("dt (seconds)"), NoUI());
    FEM_REGISTER_PROPERTY(PDEComponent, time_speed_, DisplayName("Speed"), NoUI());
    FEM_REGISTER_PROPERTY(PDEComponent, time_record_history_, DisplayName("Record History"), NoUI());
    FEM_REGISTER_PROPERTY(PDEComponent, history_max_frames_, DisplayName("History Frames"), NoUI());
}
FEM_END_PROPERTY_REGISTER(PDEComponent);

FEM_DEFINE_ENUM(DifferentialEquationSolutionMethod);

PDEComponent::PDEComponent() 
    : equation_preset_(PDEPreset::default_preset()) {
}

PDEComponent::~PDEComponent() {
    destroy_object(equation_preset_);
    for (auto& [mesh, preset] : mesh_presets_) {
        if (preset && preset != equation_preset_) [[unlikely]] {
            destroy_object(preset);
        }
    }
    mesh_presets_.clear();
}

const DifferentialEquationSolution& PDEComponent::solve(MeshComponent* target_mesh) {
    if (!target_mesh) [[unlikely]] {
        target_mesh = entity_->get_component<MeshComponent>();
        if (!target_mesh) [[unlikely]] {
            LOGT_ERROR(LogMath, "PDEComponent::solve(): No valid MeshComponent!");
            solution_.invalidate();
            return solution_;
        }
    }

    PDEPreset* preset = get_mesh_preset(target_mesh);
    if (!preset) [[unlikely]] {
        LOGT_ERROR(LogMath, "PDEComponent::solve(): No preset for mesh!");
        auto& cached_sol = mesh_solutions_[target_mesh];
        cached_sol.invalidate();
        return cached_sol.solution;
    }

    auto& cached_sol = mesh_solutions_[target_mesh];

    if (!preset->is_stationary()) {
        const void* preset_tag = static_cast<const void*>(preset->get_type_info());
        if (cached_sol.transient_preset_tag != preset_tag) {
            cached_sol.invalidate();
            cached_sol.transient_preset_tag = preset_tag;
        }

        if (!cached_sol.transient_mesh) {
            cached_sol.transient_mesh = std::make_unique<FEMMesh>(target_mesh->build_fem_mesh());
        }

        const FEMMesh& fem_mesh = *cached_sol.transient_mesh;
        const int N = fem_mesh.dof_count();

        const bool is_wave_newmark = preset->solve_kind() == SolveKind::WaveNewmark || preset->is_exactly<PDEPreset_Wave>();

        if (!cached_sol.transient_initialized || (int)cached_sol.solution.solution_u.size() != N) {
            cached_sol.solution.solution_u.assign((size_t)N, 0.0);
            for (int i = 0; i < N; ++i) {
                const auto& node = fem_mesh.nodes[(size_t)i];
                if (preset->has_initial_condition()) [[likely]] {
                    cached_sol.solution.solution_u[(size_t)i] = preset->evaluate_initial_condition(node.x, node.y);
                }
            }

            const DirichletData D = build_dirichlet_data(fem_mesh);
            for (int i = 0; i < N; ++i) {
                if (D.is_dirichlet[(size_t)i]) {
                    cached_sol.solution.solution_u[(size_t)i] = D.value[(size_t)i];
                }
            }

            if (!cached_sol.solution.solution_u.empty()) {
                auto [mn, mx] = std::minmax_element(cached_sol.solution.solution_u.begin(), cached_sol.solution.solution_u.end());
                cached_sol.solution.u_min = *mn;
                cached_sol.solution.u_max = *mx;
            }

            cached_sol.transient_time = 0.0;
            cached_sol.transient_initialized = true;
            cached_sol.min_max_valid = false;

            cached_sol.transient_v.clear();
            cached_sol.transient_a.clear();
            if (is_wave_newmark) {
                cached_sol.transient_v.assign((size_t)N, 0.0);
                cached_sol.transient_a.assign((size_t)N, 0.0);
                for (int i = 0; i < N; ++i) {
                    const auto& node = fem_mesh.nodes[(size_t)i];
                    if (preset->has_initial_velocity()) [[likely]] {
                        cached_sol.transient_v[(size_t)i] = preset->evaluate_initial_velocity(node.x, node.y);
                    }
                }

                const DirichletData D = build_dirichlet_data(fem_mesh);
                for (int i = 0; i < N; ++i) {
                    if (D.is_dirichlet[(size_t)i]) {
                        cached_sol.transient_v[(size_t)i] = 0.0;
                        cached_sol.transient_a[(size_t)i] = 0.0;
                    }
                }
            }

            if (time_playback_enabled_ && time_record_history_) {
                const size_t max_frames = (history_max_frames_ > 0) ? (size_t)history_max_frames_ : 0u;
                cached_sol.clear_history();
                cached_sol.push_history(0.0, cached_sol.solution, max_frames);
            }
        }

        const double target_time = time_playback_enabled_ ? time_seconds_ : 0.0;
        const double base_dt = std::max(time_step_seconds_, 1e-6);

        if (target_time + 1e-12 < cached_sol.transient_time) {
            if (!cached_sol.history_time.empty()) {
                int best = 0;
                for (int i = 0; i < (int)cached_sol.history_time.size(); ++i) {
                    if (cached_sol.history_time[(size_t)i] <= target_time + 1e-12) best = i;
                }
                cached_sol.seek_history(best);
                cached_sol.transient_time = cached_sol.history_time[(size_t)cached_sol.history_cursor];
            } else {
                cached_sol.transient_initialized = false;
                return solve(target_mesh);
            }
        }

        auto do_step = [&](double dt) {
            const double next_t = cached_sol.transient_time + dt;
            const BoundaryModel boundary = make_boundary_model(fem_mesh);
            auto make_request = [&](double solve_time, std::span<const double> previous_state, SolveKind kind) {
                return fem::make_solve_request(*preset, PresetSolveRequestInput{
                    .time = solve_time,
                    .boundary = boundary,
                    .dt = dt,
                    .previous_state = previous_state,
                    .solve_kind = kind
                });
            };

            if (is_wave_newmark) {
                static constexpr double beta = fem::default_newmark.beta;
                static constexpr double gamma = fem::default_newmark.gamma;

                const std::vector<double> u_n = cached_sol.solution.solution_u;
                const std::vector<double> v_n = cached_sol.transient_v;
                const std::vector<double> a_n = cached_sol.transient_a;

                std::vector<double> u_pred((size_t)N);
                std::vector<double> v_pred((size_t)N);

                const double dt2 = dt * dt;
                const double u_coeff = dt2 * (0.5 - beta);
                const double v_coeff = dt * (1.0 - gamma);

                for (int i = 0; i < N; ++i) {
                    const size_t idx = (size_t)i;
                    u_pred[idx] = u_n[idx] + dt * v_n[idx] + u_coeff * a_n[idx];
                    v_pred[idx] = v_n[idx] + v_coeff * a_n[idx];
                }

                SolveRequest request = make_request(next_t, u_pred, SolveKind::WaveNewmark);

                fem::solve(request, fem_mesh, cached_sol.solution);
                if (cached_sol.solution.is_ready()) [[likely]] {
                    const std::vector<double> u_np1 = cached_sol.solution.solution_u;
                    cached_sol.transient_v.assign((size_t)N, 0.0);
                    cached_sol.transient_a.assign((size_t)N, 0.0);

                    const double inv_beta_dt2 = 1.0 / (beta * dt * dt);
                    for (int i = 0; i < N; ++i) {
                        const size_t idx = (size_t)i;
                        const double a_np1 = inv_beta_dt2 * (u_np1[idx] - u_pred[idx]);
                        cached_sol.transient_a[idx] = a_np1;
                        cached_sol.transient_v[idx] = v_pred[idx] + dt * gamma * a_np1;
                    }

                    // Clamp wave state at Dirichlet nodes.
                    const DirichletData D = build_dirichlet_data(fem_mesh);
                    for (int i = 0; i < N; ++i) {
                        if (D.is_dirichlet[(size_t)i]) {
                            cached_sol.transient_v[(size_t)i] = 0.0;
                            cached_sol.transient_a[(size_t)i] = 0.0;
                        }
                    }

                    cached_sol.transient_time = next_t;
                }

            } else {
                const std::vector<double> u_prev = cached_sol.solution.solution_u;
                SolveRequest request = make_request(next_t, u_prev, preset->solve_kind());

                fem::solve(request, fem_mesh, cached_sol.solution);
                if (cached_sol.solution.is_ready()) [[likely]] {
                    cached_sol.transient_time = next_t;
                }
            }

            cached_sol.min_max_valid = false;
            global_bounds_valid_ = false;
            if (time_playback_enabled_ && time_record_history_) {
                const size_t max_frames = (history_max_frames_ > 0) ? (size_t)history_max_frames_ : 0u;
                cached_sol.push_history(cached_sol.transient_time, cached_sol.solution, max_frames);
            }
        };

        while (cached_sol.transient_time + base_dt <= target_time + 1e-12) {
            do_step(base_dt);
        }

        const double remaining = target_time - cached_sol.transient_time;
        if (remaining > 1e-6) {
            do_step(remaining);
        }

        target_mesh->update_buffers();
        return cached_sol.solution;
    }

    cached_sol.invalidate();

    if (solution_method_ == DifferentialEquationSolutionMethod::FEM) [[likely]] {
        FEMMesh fem_mesh = target_mesh->build_fem_mesh();
        SolveRequest request = fem::make_solve_request(*preset, PresetSolveRequestInput{
            .time = time_playback_enabled_ ? time_seconds_ : 0.0,
            .boundary = make_boundary_model(fem_mesh),
            .solve_kind = preset->solve_kind()
        });

        last_sys_ = fem::solve(request, fem_mesh, cached_sol.solution);
        has_last_sys_ = cached_sol.solution.is_ready();
        last_mesh_ = std::make_unique<FEMMesh>(fem_mesh);

        int dirichlet_edges = 0;
        for (const auto& e : fem_mesh.edges_bc) {
            if (e.type == fem::BCType::Dirichlet) ++dirichlet_edges;
        }
        if (cached_sol.solution.is_ready()) {
            LOGT_DEBUG(LogMath,
                       "PDEComponent::solve(): Solved PDE for mesh (u=[%g,%g], dirichlet_edges=%d, tagged_edges=%zu)",
                       cached_sol.solution.u_min,
                       cached_sol.solution.u_max,
                       dirichlet_edges,
                       fem_mesh.edges_bc.size());
            global_bounds_valid_ = false;
        } else {
            LOGT_ERROR(LogMath,
                       "PDEComponent::solve(): SolveRequest dispatch failed (dirichlet_edges=%d, tagged_edges=%zu)",
                       dirichlet_edges,
                       fem_mesh.edges_bc.size());
        }

        if (time_playback_enabled_ && time_record_history_) {
            const size_t max_frames = (history_max_frames_ > 0) ? (size_t)history_max_frames_ : 0u;
            cached_sol.push_history(time_seconds_, cached_sol.solution, max_frames);
        }
    } else {
        LOGT_ERROR(LogMath, "PDEComponent::solve(): Unknown solution method!");
    }

    target_mesh->update_buffers();

    return cached_sol.solution;
}

bool PDEComponent::seek_history(MeshComponent* mesh, int index) {
    if (!mesh) return false;
    auto it = mesh_solutions_.find(mesh);
    if (it == mesh_solutions_.end()) return false;

    const bool ok = it->second.seek_history(index);
    if (ok) {
        mesh->update_buffers();
        global_bounds_valid_ = false;
    }
    return ok;
}

int PDEComponent::history_size(MeshComponent* mesh) const noexcept {
    if (!mesh) return 0;
    auto it = mesh_solutions_.find(mesh);
    if (it == mesh_solutions_.end()) return 0;
    return (int)it->second.history.size();
}

int PDEComponent::history_cursor(MeshComponent* mesh) const noexcept {
    if (!mesh) return -1;
    auto it = mesh_solutions_.find(mesh);
    if (it == mesh_solutions_.end()) return -1;
    return it->second.history_cursor;
}

void PDEComponent::clear_history(MeshComponent* mesh) noexcept {
    if (!mesh) return;
    auto it = mesh_solutions_.find(mesh);
    if (it == mesh_solutions_.end()) return;
    it->second.clear_history();
}

bool PDEComponent::tick(double real_dt_seconds, MeshComponent* mesh, bool combined_domain) {
    if (!time_playback_enabled_ || !time_playing_) {
        time_accum_ = 0.0;
        return false;
    }

    if (real_dt_seconds <= 0.0) return false;
    if (time_step_seconds_ <= 0.0) return false;

    time_accum_ += real_dt_seconds * time_speed_;
    bool did_solve = false;

    // Avoid spiraling if dt is huge.
    const int max_steps_per_frame = 8;
    int steps = 0;
    while (time_accum_ >= time_step_seconds_ && steps++ < max_steps_per_frame) {
        time_seconds_ += time_step_seconds_;
        time_accum_ -= time_step_seconds_;

        if (combined_domain) {
            solve_combined_domain();
        } else {
            solve(mesh);
        }
        did_solve = true;
    }

    return did_solve;
}

const DifferentialEquationSolution& PDEComponent::solve_combined_domain() {
    solution_.invalidate();
    has_last_sys_ = false;
    last_sys_ = FEMSystem{};
    last_mesh_.reset();

    MeshComponent* primary_mesh = entity_->get_component<MeshComponent>();
    if (!primary_mesh) [[unlikely]] {
        LOGT_ERROR(LogMath, "PDEComponent::solve_combined_domain(): No primary mesh!");
        return solution_;
    }

    PlanarMathEntity* math_entity = nullptr;
    if (entity_->is_a<PlanarMathEntity>()) [[unlikely]] {
        math_entity = static_cast<PlanarMathEntity*>(entity_);
    }

    if (!math_entity || math_entity->mesh_components().empty()) [[unlikely]] {
        return solve(primary_mesh);
    }

    const auto& mesh_comps = math_entity->mesh_components();
    solution_.solution_u.reserve(mesh_comps.size() * 1000);

    bool any_valid = false;
    double combined_min = std::numeric_limits<double>::max();
    double combined_max = std::numeric_limits<double>::lowest();

    for (MeshComponent* mesh_comp : mesh_comps) {
        const auto& sub_solution = solve(mesh_comp);
        if (!sub_solution.is_ready()) [[unlikely]] {
            continue;
        }
        any_valid = true;
        combined_min = std::min(combined_min, sub_solution.u_min);
        combined_max = std::max(combined_max, sub_solution.u_max);
    }

    if (!any_valid) [[unlikely]] {
        LOGT_WARN(LogMath, "PDEComponent::solve_combined_domain(): No valid submesh solutions!");
        return solution_;
    }

    solution_.u_min = combined_min;
    solution_.u_max = combined_max;
    solution_.solution_u.push_back(1.0);
    
    global_bounds_valid_ = false;
    return solution_;
}

const DifferentialEquationSolution& PDEComponent::solution(MeshComponent* mesh) const noexcept {
    if (!mesh) [[likely]] {
        return solution_;
    }

    auto it = mesh_solutions_.find(mesh);
    if (it != mesh_solutions_.end()) [[likely]] {
        return it->second.solution;
    }

    static DifferentialEquationSolution empty_solution;
    return empty_solution;
}

PDEPreset* PDEComponent::get_mesh_preset(MeshComponent* mesh) const noexcept {
    if (!mesh) [[unlikely]] {
        return equation_preset_;
    }

    auto it = mesh_presets_.find(mesh);
    if (it != mesh_presets_.end()) [[unlikely]] {
        return it->second;
    }

    return equation_preset_;
}

void PDEComponent::set_mesh_preset(MeshComponent* mesh, PDEPreset* preset) noexcept {
    if (!mesh || !preset) [[unlikely]] {
        return;
    }

    auto& stored = mesh_presets_[mesh];
    if (stored && stored != equation_preset_ && stored != preset) {
        destroy_object(stored);
    }
    stored = preset;
    global_bounds_valid_ = false;
}

std::pair<double, double> PDEComponent::get_global_bounds() const noexcept {
    if (global_bounds_valid_) [[likely]] {
        return {cached_global_min_, cached_global_max_};
    }

    double global_min = std::numeric_limits<double>::max();
    double global_max = std::numeric_limits<double>::lowest();

    if (solution_.is_ready()) [[unlikely]] {
        global_min = solution_.u_min;
        global_max = solution_.u_max;
    }

    if (!mesh_solutions_.empty()) [[likely]] {
        for (const auto& [mesh, cached] : mesh_solutions_) {
            if (!cached.is_ready()) [[unlikely]] {
                continue;
            }
            const auto [min_val, max_val] = cached.get_bounds();
            global_min = std::min(global_min, min_val);
            global_max = std::max(global_max, max_val);
        }
    }

    if (global_min > global_max) [[unlikely]] {
        global_min = 0.0;
        global_max = 0.0;
    }

    cached_global_min_ = global_min;
    cached_global_max_ = global_max;
    global_bounds_valid_ = true;

    return {global_min, global_max};
}

void PDEComponent::for_each_parameter(void (*callback)(PDEParameter*)) const noexcept {
    if (!equation_preset_) [[unlikely]] {
        return;
    }
    equation_preset_->for_each_parameter(callback);
}

void PDEComponent::fill_fem_problem(FEMProblem& prob) const noexcept {
    if (!equation_preset_) [[unlikely]] {
        prob.a.set_constant(1.0);
        prob.c.set_constant(0.0);
        prob.f.set_constant(0.0);
        prob.set_operator_spec(LocalEllipticSpec{});
        return;
    }

    equation_preset_->apply(prob);
    prob.set_operator_spec(equation_preset_->operator_spec(prob));
}

const IReferenceProvider* PDEComponent::reference_provider() const noexcept {
    return equation_preset_ ? equation_preset_->reference_provider() : nullptr;
}

bool PDEComponent::has_exact_solution() const {
    if (!equation_preset_) return false;
    return equation_preset_->has_exact_solution();
}

bool PDEComponent::get_exact_solution(double x, double y, double& u_exact,
                                     double* ux_exact, double* uy_exact) const {
    if (!equation_preset_) return false;
    return equation_preset_->evaluate_exact_solution(x, y, u_exact, ux_exact, uy_exact);
}


}
