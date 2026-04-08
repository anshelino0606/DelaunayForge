#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "canvas_inspector.h"
#include "math/differential_equation_solution.h"
#include "math/fem/fem_mesh.h"
#include "math/fem/fem_error_analysis.h"
#include "math/fem/fem_balance_metrics.h"
#include "math/fem/fem_energy_metrics.h"
#include "math/fem/fem_self_tests.h"
#include "math/fem/field/fem_reference_provider.h"
#include "math/fem/field/fem_convergence_study.h"
#include "math/fractional_equation_config.h"

namespace fem {

class IReferenceProvider;
class PlanarMeshComponent;
class PDEComponent;
struct DelaunayTriangulationResult;

class FEMErrorAnalysisWindow final {
public:
    struct DrawInfo {
        const PlanarMeshComponent*           mesh = nullptr;
        const PDEComponent*                  pde  = nullptr;
        const DifferentialEquationSolution*  sol  = nullptr;
        const CRS*                           A    = nullptr;
        const IReferenceProvider*            ref  = nullptr;
        CanvasInspector::Selection           sel;
    };

    void draw(const DrawInfo& info);

    bool visible = true;

private:
    void ensure_cache_(const PlanarMeshComponent& mesh);
    bool try_fill_exact_(ExactSolution& out, const PDEComponent* pde) const;
    bool selection_point_(const DelaunayTriangulationResult& R,
                          const CanvasInspector::Selection& sel,
                          double& x, double& y) const;
    
    void draw_section_mesh_info_(const DrawInfo& info);
    void draw_section_local_analysis_(const DrawInfo& info);
    void draw_section_global_norms_(const DrawInfo& info);
    void draw_section_convergence_study_(const DrawInfo& info);
    void draw_section_aitken_analysis_(const DrawInfo& info);
    void draw_section_export_(const DrawInfo& info);
    void draw_section_aitken_stress_(const DrawInfo& info);
    void draw_section_flux_analysis_(const DrawInfo& info);
    void draw_section_self_tests_(const DrawInfo& info);
    void draw_section_fractional_comparison_(const DrawInfo& info);
    
    const PlanarMeshComponent* cached_mesh_ = nullptr;
    std::size_t cached_point_count_ = 0;
    std::size_t cached_tri_count_ = 0;
    FEMMesh cached_fem_;
    TriLocator locator_;

    bool probe_locked_ = false;
    double probe_x_ = 0.0;
    double probe_y_ = 0.0;
    LocalErrorResult current_local_error_;
    bool has_local_error_ = false;

    bool has_global_error_ = false;
    ErrorMetrics global_error_;
    
    ConvergenceStudyEngine study_engine_;
    ConvergenceStudyConfig study_config_;
    ConvergenceStudyResults study_results_;
    bool study_running_ = false;
    
    struct AitkenCapture {
        bool valid = false;
        double h = 0.0;
        double point_value = 0.0;
        double linf = 0.0;
        double l2 = 0.0;
        double h1_semi = 0.0;
        int dofs = 0;
        
        FEMMesh mesh;
        std::vector<double> solution;
    };
    
    AitkenCapture aitken_captures_[3]{};
    std::string aitken_status_message_;

    struct AitkenStressConfig {
        int min_level = 0;
        int max_level = 3;
        int ref_level = 4;

        int sample_points = 64;
        unsigned int seed = 1;
        bool include_pointwise = true;
        bool include_global_l2 = false;
    } stress_cfg_;

    std::string stress_status_message_;
    
    std::string export_path_ = "fem_error_analysis";
    bool show_export_dialog_ = false;

    bool has_balance_ = false;
    BalanceMetrics balance_;
    BalanceMetricsConfig balance_cfg_;

    bool self_test_robin_ran_   = false;
    bool self_test_mms_ran_     = false;
    RobinSlabTestResult  robin_result_;
    MMSConvergenceResult mms_result_;
    float mms_kappa_ = 1.0f;

    struct FracCompare {
        bool   ran = false;
        float  s   = 0.5f;          // fractional order

        bool   use_spectral = true;
        bool   use_integral = true;
        bool   use_regional = false;

        static constexpr int SLOT_CLASSICAL = 0;
        static constexpr int SLOT_SPECTRAL  = 1;
        static constexpr int SLOT_INTEGRAL  = 2;
        static constexpr int SLOT_REGIONAL  = 3;
        static constexpr int NUM_SLOTS      = 4;

        struct SlotResult {
            bool             active = false;
            const char*      label  = "";
            EnergyMetrics    energy;
            ErrorMetrics     err;
            std::vector<double> u;
            double           inner_avg_u = 0.0;
        };
        SlotResult slots[NUM_SLOTS]{};

        // BC diagnostics
        int    n_robin_edges = 0;
        double robin_perimeter = 0.0;
        double robin_avg_k = 0.0;
        double robin_avg_g = 0.0;
        bool   robin_k_is_zero = false;
        int    n_dirichlet_edges = 0;
        int    n_neumann_edges = 0;

        std::atomic<bool> running{false};
        std::atomic<int>  progress{0};     // 0..NUM_SLOTS
        std::thread       worker;

        FracCompare() {
            slots[SLOT_CLASSICAL].label = "Classical";
            slots[SLOT_SPECTRAL].label  = "Spectral s";
            slots[SLOT_INTEGRAL].label  = "Integral s";
            slots[SLOT_REGIONAL].label  = "Regional s";
        }
        ~FracCompare() { join(); }
        void join() { if (worker.joinable()) worker.join(); }

        // Move-only (std::atomic is not copyable/movable)
        FracCompare(const FracCompare&) = delete;
        FracCompare& operator=(const FracCompare&) = delete;
        FracCompare(FracCompare&& o) noexcept
            : slots{std::move(o.slots[0]), std::move(o.slots[1]),
                    std::move(o.slots[2]), std::move(o.slots[3])}
            , n_robin_edges(o.n_robin_edges)
            , robin_perimeter(o.robin_perimeter)
            , robin_avg_k(o.robin_avg_k)
            , robin_avg_g(o.robin_avg_g)
            , robin_k_is_zero(o.robin_k_is_zero)
            , n_dirichlet_edges(o.n_dirichlet_edges)
            , n_neumann_edges(o.n_neumann_edges)
            , running(o.running.load())
            , progress(o.progress.load())
            , worker(std::move(o.worker))
        {}
        FracCompare& operator=(FracCompare&& o) noexcept {
            if (this != &o) {
                join();
                for (int i = 0; i < NUM_SLOTS; ++i) slots[i] = std::move(o.slots[i]);
                n_robin_edges = o.n_robin_edges;
                robin_perimeter = o.robin_perimeter;
                robin_avg_k = o.robin_avg_k;
                robin_avg_g = o.robin_avg_g;
                robin_k_is_zero = o.robin_k_is_zero;
                n_dirichlet_edges = o.n_dirichlet_edges;
                n_neumann_edges = o.n_neumann_edges;
                running.store(o.running.load());
                progress.store(o.progress.load());
                worker = std::move(o.worker);
            }
            return *this;
        }
    } frac_cmp_;
};

} // namespace fem
