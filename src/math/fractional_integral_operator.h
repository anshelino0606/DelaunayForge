#pragma once

#include "pde/operator_spec.h"
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <vector>

namespace fem {

struct FEMMesh;
struct FEMProblem;

// Canonical name for the editor-facing 3x3 nonlocal block extracted on one
// triangle. This is not the global fractional operator used by assembly.
class FractionalElementContribution {
public:
    FractionalElementContribution(const glm::ivec3& global_vertex_indices);

    void compute(
        const FEMMesh& mesh, const FractionalIntegralSpec& spec,
        const FEMProblem& prob, const std::vector<double>& nodal_mass
    );

    void compute(
        const FEMMesh& mesh, const FractionalRegionalSpec& spec,
        const FEMProblem& prob, const std::vector<double>& nodal_mass
    );

    const glm::dmat3& Af() const { return Af_; }
    const glm::dvec3& bf() const { return bf_; }

private:
    glm::dmat3 Af_{0};
    glm::dvec3 bf_{0};
    glm::ivec3 vertex_indices_{0};
};

using FractionalIntegralOperator = FractionalElementContribution;

}