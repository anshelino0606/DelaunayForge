#pragma once
#ifdef USE_BGFX

#include "common.h"
#include "bgfx_mesh.h"
#include "geom/mesh_generator.h"
#include "rhi/graphics_program.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <vector>

namespace fem {

class MeshBgfxAdapter {
public:
    explicit MeshBgfxAdapter(const fem::GraphicsProgram& program)
        : program_(program) {}

    void generate_mesh(const std::shared_ptr<MeshGeneratorStrategy>& gen) {
        if (!gen) return;
        vertices_ = gen->generateMesh();
        indices_  = gen->getIndices();
        mesh_.update(vertices_, indices_);
    }

    void update(const std::vector<glm::vec3>& verts,
                const std::vector<unsigned>& idx) {
        vertices_ = verts; indices_ = idx;
        mesh_.update(vertices_, indices_);
    }

    void reset() {
        vertices_.clear(); indices_.clear();
        mesh_.destroy();
    }

    void set_color(const glm::vec4& c) { mesh_.setColor(c.r, c.g, c.b, c.a); }

    void draw(uint64_t state = 0) const
    {
        mesh_.submit(program_.handle(), fem::FEM_VIEW_MODE_3D, state);
    }

    const std::vector<glm::vec3>& get_vertices() const { return vertices_; }
    const std::vector<unsigned>&  get_indices()  const { return indices_;  }

private:
    BgfxMesh mesh_;
    fem::GraphicsProgram program_;
    std::vector<glm::vec3> vertices_;
    std::vector<unsigned>  indices_;
};

}

#endif // USE_BGFX
