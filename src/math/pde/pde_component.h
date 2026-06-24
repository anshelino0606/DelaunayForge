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

    [[nodiscard]] bool is_ready() const noexcept;

    void invalidate() noexcept;

    [[nodiscard]] bool has_history() const noexcept;

    [[nodiscard]] const DifferentialEquationSolution& current_solution() const noexcept;

    void clear_history() noexcept;

    void push_history(double t, const DifferentialEquationSolution& sol, size_t max_frames);

    bool seek_history(int idx);

    [[nodiscard]] std::pair<double, double> get_bounds() const noexcept;
};

class PDEComponent : public Component {
public:
    FEM_DECLARE_OBJECT(PDEComponent);
    FEM_DECLARE_PROPERTY_REGISTER(PDEComponent);

    using MeshSolutionMap = std::unordered_map<MeshComponent*, CachedSolution>;
    using MeshPresetMap = std::unordered_map<MeshComponent*, PDEPreset*>;

    const DifferentialEquationSolution& solve(MeshComponent* mesh = nullptr);
    const DifferentialEquationSolution& solve_combined_domain();

    [[nodiscard]] bool time_playback_enabled() const noexcept;
    [[nodiscard]] bool time_playing() const noexcept;
    [[nodiscard]] double time_seconds() const noexcept;
    [[nodiscard]] double time_step_seconds() const noexcept;
    [[nodiscard]] double time_speed() const noexcept;
    [[nodiscard]] bool time_record_history() const noexcept;
    [[nodiscard]] int32_t history_max_frames() const noexcept;

    void set_time_playback_enabled(bool v) noexcept;
    void set_time_seconds(double t) noexcept;
    void set_time_playing(bool v) noexcept;
    void set_time_step_seconds(double dt) noexcept;
    void set_time_speed(double s) noexcept;
    void set_time_record_history(bool v) noexcept;
    void set_history_max_frames(int32_t n) noexcept;

    bool tick(double real_dt_seconds, MeshComponent* mesh, bool combined_domain);

    bool seek_history(MeshComponent* mesh, int index);
    int history_size(MeshComponent* mesh) const noexcept;
    int history_cursor(MeshComponent* mesh) const noexcept;
    void clear_history(MeshComponent* mesh) noexcept;

    void for_each_parameter(void (*callback)(PDEParameter*)) const noexcept;

    PDEComponent();
    ~PDEComponent();

    [[nodiscard]] DifferentialEquationSolutionMethod solution_method() const noexcept;

    [[nodiscard]] const DifferentialEquationSolution& solution(MeshComponent* mesh = nullptr) const noexcept;

    [[nodiscard]] const MeshSolutionMap& all_solutions() const noexcept;

    [[nodiscard]] std::pair<double, double> get_global_bounds() const noexcept;

    [[nodiscard]] PDEPreset* get_mesh_preset(MeshComponent* mesh) const noexcept;
    void set_mesh_preset(MeshComponent* mesh, PDEPreset* preset) noexcept;

    void fill_fem_problem(FEMProblem& prob) const noexcept;

    void invalidate_all_solutions() noexcept;

    [[nodiscard]] const FEMSystem* last_system() const noexcept;
    
    [[nodiscard]] const FEMMesh* last_mesh() const noexcept;

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

