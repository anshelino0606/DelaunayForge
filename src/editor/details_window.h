#ifndef FEM_DETAILS_WINDOW_H
#define FEM_DETAILS_WINDOW_H

#include "tools/fem_matrix_assembly_preview.h"
#include <cstdint>
#include <memory>

namespace fem {

class Entity;
class BoundaryCondition;
class PlanarMeshComponent;
class PDEComponent;
class FEMMesh;
struct CanvasWindowState;

struct DetailsWindowDrawInfo {
    Entity* selected_entity = nullptr;
    PlanarMeshComponent* selected_mesh = nullptr;
    CanvasWindowState* canvas_state = nullptr;
};

class DetailsWindow {
public:
    DetailsWindow();
    ~DetailsWindow();
    void draw(const DetailsWindowDrawInfo& draw_info);

    void reset() noexcept;

private:
    Entity* last_selected_entity_ = nullptr;
    PlanarMeshComponent* last_selected_mesh_ = nullptr;
    BoundaryCondition* selected_boundary_condition_ = nullptr;

    MatrixPreviewData preview_{};
    bool has_preview_ = false;
    PreviewConfig preview_cfg_{ .max_size = 256, .include_values = false };

    std::unique_ptr<FEMMesh> fem_mesh_cache_;

    void setup_pde_draw_callbacks();

    void draw_preview_ui();
    void assemble_preview(PDEComponent& pde, PlanarMeshComponent& mesh);

    struct ActiveContext {
        DetailsWindow* self = nullptr;
        const DetailsWindowDrawInfo* di = nullptr;
    };
    static thread_local ActiveContext* tls_;

    struct ScopedContext {
        ActiveContext ctx;
        ScopedContext(DetailsWindow& w, const DetailsWindowDrawInfo& di) {
            ctx.self = &w;
            ctx.di = &di;
            tls_ = &ctx;
        }
        ~ScopedContext() { tls_ = nullptr; }
        ScopedContext(const ScopedContext&) = delete;
        ScopedContext& operator=(const ScopedContext&) = delete;
    };
};

} // namespace fem

#endif
