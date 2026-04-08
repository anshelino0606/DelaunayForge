#ifndef FEM_MESH_COMPONENT_H
#define FEM_MESH_COMPONENT_H

#include "core/entity/component.h"
#include "math/fem/fem_mesh.h"
#include <LLGL/LLGL.h>

#include <utility>

namespace fem {

class BoundaryCondition;

class MeshComponent : public Component {
public:
    FEM_DECLARE_OBJECT(MeshComponent);
    FEM_DECLARE_PROPERTY_REGISTER(MeshComponent);

    virtual ~MeshComponent() override;

    virtual FEMMesh build_fem_mesh() const { return FEMMesh(); }

    const std::vector<BoundaryCondition*>& boundary_conditions() const {
        return boundary_conditions_;
    }

    LLGL::Buffer* vertex_buffer() const { return vertex_buffer_; }
    LLGL::Buffer* index_buffer() const { return index_buffer_; }
    virtual uint32_t index_count() const { return 0; };

    virtual void update_buffers() {}

    [[nodiscard]] bool has_display_u_bounds() const noexcept { return display_u_bounds_valid_; }
    [[nodiscard]] std::pair<float, float> display_u_bounds() const noexcept { return {display_u_min_, display_u_max_}; }

protected:
    std::vector<BoundaryCondition*> boundary_conditions_;

    LLGL::Buffer* vertex_buffer_ = nullptr;
    LLGL::Buffer* index_buffer_ = nullptr;

    void set_display_u_bounds(float u_min, float u_max) noexcept {
        display_u_min_ = u_min;
        display_u_max_ = u_max;
        display_u_bounds_valid_ = true;
    }

    void clear_display_u_bounds() noexcept {
        display_u_bounds_valid_ = false;
    }

private:
    bool  display_u_bounds_valid_ = false;
    float display_u_min_ = 0.0f;
    float display_u_max_ = 1.0f;
};

}

#endif // FEM_MESH_COMPONENT_H