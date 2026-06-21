#pragma once

#include "fractional_equation_config.h"
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace fem {

struct FEMMesh;
struct FEMProblem;

class FractionalIntegralOperator {
public:
    FractionalIntegralOperator(const glm::ivec3& global_vertex_indices);

    void compute(
        const FEMMesh& mesh, const FractionalEquationConfig& cfg,
        const FEMProblem& prob, const std::vector<double>& nodal_mass
    );

    const glm::dmat3& Af() const { return Af_; }
    const glm::dvec3& bf() const { return bf_; }

private:
    glm::dmat3 Af_{0};
    glm::dvec3 bf_{0};
    glm::ivec3 vertex_indices_{0};
};

}