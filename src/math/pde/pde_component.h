#ifndef FEM_PDE_COMPONENT_H
#define FEM_PDE_COMPONENT_H

#include "core/entity/component.h"
#include "math/differential_equation_solution.h"
#include "math/fem/fem_problem.h"
#include <functional>
#include <memory>
#include "math/fem/fem_assembler.h"
#include "math/fem/field/fem_reference_provider.h"
#include <unordered_map>

namespace fem {

class PDEPreset;
class PDEParameter;
class MeshComponent;

FEM_DECLARE_ENUM(DifferentialEquationSolutionMethod, FEM);

struct CachedSolution {
    DifferentialEquationSolution solution;
    mutable bool min_max_valid = false;
    mutable double cached_min = 0.0;
    mutable double cached_max = 0.0;

    std::vector<DifferentialEquationSolution> history;
    std::vector<double> history_time;
    int history_cursor = -1;

    std::unique_ptr<FEMMesh> transient_mesh;
    double transient_time = 0.0;
    bool transient_initialized = false;

    const void* transient_preset_tag = nullptr;

    std::vector<double> transient_v;
    std::vector<double> transient_a;

    [[nodiscard]] inline bool is_ready() const noexcept { 
        return solution.is_ready(); 
    }

    inline void invalidate() noexcept {
        solution.invalidate();
        min_max_valid = false;
        history.clear();
        history_time.clear();
        history_cursor = -1;

        transient_mesh.reset();
        transient_time = 0.0;
        transient_initialized = false;

        transient_preset_tag = nullptr;
        transient_v.clear();
        transient_a.clear();
    }

    [[nodiscard]] inline bool has_history() const noexcept {
        return history_cursor >= 0 && history_cursor < (int)history.size();
    }

    [[nodiscard]] inline const DifferentialEquationSolution& current_solution() const noexcept {
        return has_history() ? history[(size_t)history_cursor] : solution;
    }

    inline void clear_history() noexcept {
        history.clear();
        history_time.clear();
        history_cursor = -1;
    }

    inline void push_history(double t, const DifferentialEquationSolution& sol, size_t max_frames) {
        if (max_frames == 0) return;

        if (history.size() >= max_frames) {
            // Drop oldest frame.
            history.erase(history.begin());
            history_time.erase(history_time.begin());
            if (history_cursor > 0) {
                --history_cursor;
            }
        }

        history.push_back(sol);
        history_time.push_back(t);
        history_cursor = (int)history.size() - 1;
    }

    inline bool seek_history(int idx) {
        if (idx < 0 || idx >= (int)history.size()) return false;
        history_cursor = idx;
        solution = history[(size_t)history_cursor];
        min_max_valid = false;
        return true;
    }

    [[nodiscard]] inline std::pair<double, double> get_bounds() const noexcept {
        if (min_max_valid && solution.is_ready()) [[likely]] {
            return {cached_min, cached_max};
        }
        if (solution.is_ready()) [[likely]] {
            cached_min = solution.u_min;
            cached_max = solution.u_max;
            min_max_valid = true;
        }
        return {cached_min, cached_max};
    }
};

class PDEComponent : public Component {
public:
    FEM_DECLARE_OBJECT(PDEComponent);
    FEM_DECLARE_PROPERTY_REGISTER(PDEComponent);

    using MeshSolutionMap = std::unordered_map<MeshComponent*, CachedSolution>;
    using MeshPresetMap = std::unordered_map<MeshComponent*, PDEPreset*>;

    const DifferentialEquationSolution& solve(MeshComponent* mesh = nullptr);
    const DifferentialEquationSolution& solve_combined_domain();

    [[nodiscard]] bool time_playback_enabled() const noexcept { return time_playback_enabled_; }
    [[nodiscard]] bool time_playing() const noexcept { return time_playing_; }
    [[nodiscard]] double time_seconds() const noexcept { return time_seconds_; }
    [[nodiscard]] double time_step_seconds() const noexcept { return time_step_seconds_; }
    [[nodiscard]] double time_speed() const noexcept { return time_speed_; }
    [[nodiscard]] bool time_record_history() const noexcept { return time_record_history_; }
    [[nodiscard]] int32_t history_max_frames() const noexcept { return history_max_frames_; }

    void set_time_playback_enabled(bool v) noexcept { 
        time_playback_enabled_ = v; 
        if (!time_playback_enabled_) {
            time_playing_ = false;
        }
    }
    void set_time_seconds(double t) noexcept { time_seconds_ = t; }
    void set_time_playing(bool v) noexcept { time_playing_ = v; }
    void set_time_step_seconds(double dt) noexcept { time_step_seconds_ = dt; }
    void set_time_speed(double s) noexcept { time_speed_ = s; }
    void set_time_record_history(bool v) noexcept { time_record_history_ = v; }
    void set_history_max_frames(int32_t n) noexcept { history_max_frames_ = n; }

    bool tick(double real_dt_seconds, MeshComponent* mesh, bool combined_domain);

    bool seek_history(MeshComponent* mesh, int index);
    int history_size(MeshComponent* mesh) const noexcept;
    int history_cursor(MeshComponent* mesh) const noexcept;
    void clear_history(MeshComponent* mesh) noexcept;

    void for_each_parameter(void (*callback)(PDEParameter*)) const noexcept;

    PDEComponent();
    ~PDEComponent();

    [[nodiscard]] inline DifferentialEquationSolutionMethod solution_method() const noexcept {
        return solution_method_;
    }

    [[nodiscard]] const DifferentialEquationSolution& solution(MeshComponent* mesh = nullptr) const noexcept;

    [[nodiscard]] inline const MeshSolutionMap& all_solutions() const noexcept {
        return mesh_solutions_;
    }

    [[nodiscard]] std::pair<double, double> get_global_bounds() const noexcept;

    [[nodiscard]] PDEPreset* get_mesh_preset(MeshComponent* mesh) const noexcept;
    void set_mesh_preset(MeshComponent* mesh, PDEPreset* preset) noexcept;

    void fill_fem_problem(FEMProblem& prob) const noexcept;

    inline void invalidate_all_solutions() noexcept {
        solution_.invalidate();
        for (auto& [mesh, cached] : mesh_solutions_) {
            cached.invalidate();
        }
        global_bounds_valid_ = false;
    }

    [[nodiscard]] const FEMSystem* last_system() const noexcept { 
        return has_last_sys_ ? &last_sys_ : nullptr; 
    }
    
    [[nodiscard]] const FEMMesh* last_mesh() const noexcept { 
        return last_mesh_.get(); 
    }

    const IReferenceProvider* reference_provider() const noexcept;

    bool has_exact_solution() const;
    bool get_exact_solution(double x, double y, double& u_exact, 
                           double* ux_exact = nullptr, 
                           double* uy_exact = nullptr) const;
    

protected:
    PDEPreset* equation_preset_;
    MeshPresetMap mesh_presets_;
    DifferentialEquationSolution solution_;
    MeshSolutionMap mesh_solutions_;
    mutable bool global_bounds_valid_ = false;
    mutable double cached_global_min_ = 0.0;
    mutable double cached_global_max_ = 0.0;
    DifferentialEquationSolutionMethod solution_method_ = DifferentialEquationSolutionMethod::FEM;

    bool   time_playback_enabled_ = false;
    bool   time_playing_ = false;
    bool   time_record_history_ = true;
    int32_t history_max_frames_ = 240;
    double time_seconds_ = 0.0;
    double time_step_seconds_ = 0.05;
    double time_speed_ = 1.0;
    double time_accum_ = 0.0;


    std::unique_ptr<FEMMesh> last_mesh_;
    FEMSystem                last_sys_;
    bool                     has_last_sys_ = false;
};

}

#endif // FEM_PDE_COMPONENT_H

