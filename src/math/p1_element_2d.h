#pragma once

#include "fem/fem_mesh.h"
#include <glm/glm.hpp>
#include <array>

namespace fem {

struct FEMProblem;
struct Point2D;

class P1Element2D {
public:
    P1Element2D(const Point2D& p0, const Point2D& p1, const Point2D& p2, const FEMProblem& problem);

    // If bc is nullptr, returns and does nothing
    void apply_edge_bc(const FEMMesh::EdgeBC* bc, uint32_t local_a, uint32_t local_b);
    bool apply_dirichlet_elimination(const std::vector<uint8_t>& is_dirichlet, const std::vector<double>& dirichlet_values);

    double area() const { return area_; }
    double a_coeff() const { return a_coeff_; }
    double c_coeff() const { return c_coeff_; }
    double f_coeff() const { return f_coeff_; }
    const glm::dvec2& centroid() const { return centroid_; }
    const glm::dmat3& Ke() const { return Ke_; }
    const glm::dmat3& Ce() const { return Ce_; }
    const glm::dmat3& Ae() const { return Ae_; }
    const glm::dmat3& Ae_eff() const { return Ae_eff_; }
    const glm::dvec3& be() const { return be_; }
    const glm::dvec3& be_eff() const { return be_eff_; }
    const glm::ivec3& dirichlet_vertices() const { return dirichlet_vertices_; }

    bool is_area_valid() const { return area_ >= 1e-30; }

private:
    glm::dmat3 Ke_{0.0};
    glm::dmat3 Ce_{0.0};
    glm::dmat3 Ae_{0.0};
    glm::dmat3 Ae_eff_{0.0};
    glm::dvec3 be_{0.0};
    glm::dvec3 be_eff_{0.0};
    glm::dvec3 b_{0.0};
    glm::dvec3 c_{0.0};
    glm::dvec2 centroid_{0.0};
    glm::ivec3 dirichlet_vertices_{0};
    double area_ = 0.0;
    double a_coeff_ = 0.0;
    double c_coeff_ = 0.0;
    double f_coeff_ = 0.0;

    std::array<const Point2D*, 3> points_;

    void compute_element();

    void add_robin_edge(uint32_t local_a, uint32_t local_b, double L, double k, double g);
    void add_neumann_edge(uint32_t local_a, uint32_t local_b, double L, double gN);
};

}