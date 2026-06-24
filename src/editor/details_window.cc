#include "details_window.h"
#include "canvas_window.h"
#include "core/entity/entity.h"
#include "math/boundary_condition.h"
#include "math/pde/pde_component.h"
#include "math/pde/pde_preset.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "math/fem/fem_problem.h"
#include "math/fem/fem_mesh.h"
#include "math/entities/planar_math_entity.h"
#include "widgets/object_widget.h"
#include <imgui/imgui.h>

#include <algorithm>

namespace fem {

thread_local DetailsWindow::ActiveContext* DetailsWindow::tls_ = nullptr;

namespace {

template <class Pde>
const DifferentialEquation* try_get_equation(const Pde& pde) noexcept {
    if constexpr (requires { pde.equation(); }) {
        return &pde.equation();
    } else if constexpr (requires { pde.get_equation(); }) {
        return &pde.get_equation();
    } else {
        return nullptr;
    }
}

} // namespace

DetailsWindow::DetailsWindow() {
    setup_pde_draw_callbacks();
}

DetailsWindow::~DetailsWindow() = default;

void DetailsWindow::reset() noexcept {
    last_selected_entity_ = nullptr;
    last_selected_mesh_ = nullptr;
    selected_boundary_condition_ = nullptr;

    has_preview_ = false;
    preview_ = {};
    preview_cfg_ = { .max_size = 256, .include_values = false };

    fem_mesh_cache_.reset();
}


void DetailsWindow::draw(const DetailsWindowDrawInfo& draw_info) {
    ScopedContext _scope(*this, draw_info);

    if (draw_info.selected_entity != last_selected_entity_) {
        last_selected_entity_ = draw_info.selected_entity;
    }

    if (draw_info.selected_mesh != last_selected_mesh_) {
        last_selected_mesh_ = draw_info.selected_mesh;
        selected_boundary_condition_ = nullptr;
        has_preview_ = false;
        fem_mesh_cache_.reset();
    }

    ImGui::Begin("Details");

    if (last_selected_entity_) {
        if (last_selected_mesh_) {
            ObjectWidget().draw(last_selected_mesh_);
        }

        if (PDEComponent* pde = last_selected_entity_->get_component<PDEComponent>()) {
            ObjectWidget().draw(pde);
        }
    }

    ImGui::End();
}

void DetailsWindow::setup_pde_draw_callbacks() {
    const ObjectTypeInfo* pde_type_info = PDEComponent::get_static_type_info();
    const DrawCallbacks* draw_callbacks = pde_type_info->get_attribute<DrawCallbacks>();
    if (!draw_callbacks) return;

    draw_callbacks->post_draw_properties = [](void* owner_object) {
        auto* ctx = DetailsWindow::tls_;
        if (!ctx || !ctx->self || !ctx->di) return;

        auto& self = *ctx->self;
        const auto& di = *ctx->di;

        auto* pde = static_cast<PDEComponent*>(owner_object);
        if (!pde) return;

        if (!di.selected_mesh) {
            ImGui::TextUnformatted("Select a mesh to assemble.");
            return;
        }

        if (ImGui::Button("Assemble (Preview)")) {
            self.assemble_preview(*pde, *di.selected_mesh);
        }

        ImGui::SameLine();
        if (ImGui::Button("Solve")) {
            // If the selected entity has multiple submeshes, solve all
            // Otherwise, solve just the selected mesh
            PlanarMathEntity* math_entity = nullptr;
            if (di.selected_entity && di.selected_entity->is_a<PlanarMathEntity>()) {
                math_entity = static_cast<PlanarMathEntity*>(di.selected_entity);
            }
            
            if (math_entity && math_entity->mesh_components().size() > 1) {
                pde->solve_combined_domain();
            } else {
                pde->solve(di.selected_mesh);
            }
        }

        // Time playback helpers (only for non-stationary presets).
        if (di.selected_mesh) {
            const PDEPreset* preset = pde->get_mesh_preset(di.selected_mesh);
            const bool is_transient = preset && !preset->is_stationary();
            if (!is_transient) {
                ImGui::SameLine();
                ImGui::Checkbox("Values", &self.preview_cfg_.include_values);

                ImGui::SameLine();
                ImGui::SetNextItemWidth(110.0f);
                ImGui::SliderInt("##prevsz", &self.preview_cfg_.max_size, 64, 512);

                self.draw_preview_ui();
                return;
            }

            const bool combined = [&]() {
                PlanarMathEntity* math_entity = nullptr;
                if (di.selected_entity && di.selected_entity->is_a<PlanarMathEntity>()) {
                    math_entity = static_cast<PlanarMathEntity*>(di.selected_entity);
                }
                return math_entity && math_entity->mesh_components().size() > 1;
            }();

            ImGui::SeparatorText("Time");

            bool playback_enabled = pde->time_playback_enabled();
            if (ImGui::Checkbox("Time Playback", &playback_enabled)) {
                pde->set_time_playback_enabled(playback_enabled);
                if (!playback_enabled) {
                    pde->set_time_playing(false);
                } else {
                    combined ? pde->solve_combined_domain() : pde->solve(di.selected_mesh);
                }
            }

            ImGui::BeginDisabled(!playback_enabled);

            double t = pde->time_seconds();
            if (ImGui::InputDouble("t (seconds)", &t, 0.0, 0.0, "%.6f")) {
                pde->set_time_playing(false);
                pde->set_time_seconds(std::max(0.0, t));
                combined ? pde->solve_combined_domain() : pde->solve(di.selected_mesh);
            }

            double dt = pde->time_step_seconds();
            if (ImGui::InputDouble("dt (seconds)", &dt, 0.0, 0.0, "%.6f")) {
                pde->set_time_step_seconds(std::max(1e-6, dt));
            }

            double speed = pde->time_speed();
            if (ImGui::InputDouble("Speed", &speed, 0.0, 0.0, "%.6f")) {
                pde->set_time_speed(std::max(0.0, speed));
            }

            bool record_history = pde->time_record_history();
            if (ImGui::Checkbox("Record History", &record_history)) {
                pde->set_time_record_history(record_history);
            }

            int max_frames = (int)pde->history_max_frames();
            if (ImGui::InputInt("History Frames", &max_frames)) {
                pde->set_history_max_frames((int32_t)std::max(0, max_frames));
            }

            // Step via history when available; otherwise step time and re-solve.
            const int hist_size = pde->history_size(di.selected_mesh);
            const int hist_cursor = pde->history_cursor(di.selected_mesh);

            if (ImGui::Button("<<")) {
                if (hist_size > 0 && hist_cursor > 0) {
                    pde->set_time_playing(false);
                    pde->seek_history(di.selected_mesh, hist_cursor - 1);
                } else {
                    pde->set_time_playing(false);
                    pde->set_time_seconds(pde->time_seconds() - pde->time_step_seconds());
                    combined ? pde->solve_combined_domain() : pde->solve(di.selected_mesh);
                }
            }
            ImGui::SameLine();

            if (ImGui::Button(pde->time_playing() ? "Pause" : "Play")) {
                pde->set_time_playing(!pde->time_playing());
            }
            ImGui::SameLine();

            if (ImGui::Button(">>")) {
                pde->set_time_playing(false);
                pde->set_time_seconds(pde->time_seconds() + pde->time_step_seconds());
                combined ? pde->solve_combined_domain() : pde->solve(di.selected_mesh);
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear History")) {
                pde->clear_history(di.selected_mesh);
            }

            if (hist_size > 0) {
                int idx = std::clamp(hist_cursor, 0, std::max(0, hist_size - 1));
                ImGui::SetNextItemWidth(240.0f);
                if (ImGui::SliderInt("History", &idx, 0, hist_size - 1)) {
                    pde->set_time_playing(false);
                    pde->seek_history(di.selected_mesh, idx);
                }
            }

            // Drive time playback and solve while playing.
            if (playback_enabled) {
                pde->tick(ImGui::GetIO().DeltaTime, di.selected_mesh, combined);
            }

            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Values", &self.preview_cfg_.include_values);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::SliderInt("##prevsz", &self.preview_cfg_.max_size, 64, 512);

        self.draw_preview_ui();
    };
}

void DetailsWindow::assemble_preview(PDEComponent& pde, PlanarMeshComponent& mesh) {
    fem_mesh_cache_ = std::make_unique<FEMMesh>(mesh.build_fem_mesh());

    FEMProblem prob;
    prob.mesh = fem_mesh_cache_.get();

    prob.a.set_constant(1.0);
    prob.c.set_constant(0.0);
    prob.f.set_constant(0.0);
    prob.set_operator_spec(LocalEllipticSpec{});

    pde.fill_fem_problem(prob);

    auto assembled = FEMMatrixAssembler<double>::assemble(prob);
    preview_ = FEMMatrixAssembler<double>::make_preview(assembled.K, assembled.stats, preview_cfg_);
    has_preview_ = preview_.valid();
}

void DetailsWindow::draw_preview_ui() {
    if (!has_preview_ || !preview_.valid()) return;

    ImGui::Separator();
    ImGui::Text("DOFs: %zu  nnz: %zu  bw: %zu  time: %.3f ms  sparsity: %.4f",
                preview_.stats.total_dofs,
                preview_.stats.matrix_nnz,
                preview_.stats.bandwidth,
                preview_.stats.assembly_time_ms,
                preview_.stats.sparsity());

    ImGui::TextUnformatted("Global matrix preview:");

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float scale = 1.0f;
    const ImVec2 sz(preview_.w * scale, preview_.h * scale);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), IM_COL32(20, 20, 20, 255));

    const int w = preview_.w;
    const int h = preview_.h;

    for (int y = 0; y < h; ++y) {
        const int row_off = y * w;
        for (int x = 0; x < w; ++x) {
            const std::size_t idx = static_cast<std::size_t>(row_off + x);
            if (!preview_.occ[idx]) continue;

            std::uint8_t intensity = 220;
            if (preview_.has_values) {
                const float v = preview_.values[idx]; // normalized [0,1]
                intensity = static_cast<std::uint8_t>(40 + 215.0f * v);
            }

            const float fx = p0.x + x * scale;
            const float fy = p0.y + y * scale;
            dl->AddRectFilled(ImVec2(fx, fy), ImVec2(fx + scale, fy + scale),
                              IM_COL32(intensity, intensity, intensity, 255));
        }
    }

    ImGui::Dummy(sz);
}

} // namespace fem
