#include "p1_element_2d.h"
#include "fem/fem_problem.h"
#include "geom/geom2d/tri.h"
#include "geom/geom2d/vec.h"
#include "geom/common_types_2d.h"

namespace fem {

P1Element2D::P1Element2D(const Point2D& p0, const Point2D& p1, const Point2D& p2, const FEMProblem& problem) {
    area_ = geom2d::tri::area(p0, p1, p2);
    if (!is_area_valid()) return;

    geom2d::tri::shape_coefficients(p0, p1, p2, b_, c_);

    centroid_ = geom2d::tri::centroid(p0, p1, p2);

    a_coeff_ = problem.a(centroid_.x, centroid_.y);
    c_coeff_ = problem.c(centroid_.x, centroid_.y);
    f_coeff_ = problem.f(centroid_.x, centroid_.y);

    points_ = { &p0, &p1, &p2 };

    compute_element();
}

void P1Element2D::apply_edge_bc(const FEMMesh::EdgeBC* bc, uint32_t local_a, uint32_t local_b) {
    if (!bc || !is_area_valid()) return;

    const double L = geom2d::vec::dist(*points_[local_a], *points_[local_b]);

    if (bc->type == fem::BCType::Robin) {
        add_robin_edge(local_a, local_b, L, bc->k, bc->g);
    } else if (bc->type == fem::BCType::Neumann) {
        add_neumann_edge(local_a, local_b, L, bc->gN);
    }
}

bool P1Element2D::apply_dirichlet_elimination(const std::vector<uint8_t>& is_dirichlet, const std::vector<double>& dirichlet_values) {
    glm::ivec3 point_ids = {
        points_[0]->id, points_[1]->id, points_[2]->id
    };

    int32_t is_dirichlet_count = (int32_t)is_dirichlet.size();

    glm::bvec3 p1_has_dirichlet{false};

    p1_has_dirichlet[0] = (point_ids[0] < is_dirichlet_count) ? (is_dirichlet[(size_t)point_ids[0]] != 0) : false;
    p1_has_dirichlet[1] = (point_ids[1] < is_dirichlet_count) ? (is_dirichlet[(size_t)point_ids[1]] != 0) : false;
    p1_has_dirichlet[2] = (point_ids[2] < is_dirichlet_count) ? (is_dirichlet[(size_t)point_ids[2]] != 0) : false;

    if (glm::any(p1_has_dirichlet)) {
        be_eff_ = be_;
        Ae_eff_ = Ae_;

        auto uD = [&](int32_t gvid) {
            if (gvid >= (int32_t)dirichlet_values.size()) return 0.0;
            return dirichlet_values[(size_t)gvid];
        };

        for (int32_t li = 0; li < 3; ++li) {
            if (p1_has_dirichlet[li]) {
                dirichlet_vertices_[li] = point_ids[li];
                continue;
            }

            for (int32_t lj = 0; lj < 3; ++lj) {
                if (!p1_has_dirichlet[lj])
                    continue;

                be_eff_[li] -= Ae_[lj][li] * uD(point_ids[lj]);
                Ae_eff_[lj][li] = 0.0;
            }
        }

        return true;
    }

    return false;
}

void P1Element2D::compute_element() {
    if (!is_area_valid()) return; 

    const double inv4A = 1.0 / (4.0 * area_);
    const double mfac  = (c_coeff_ * area_) / 12.0;
    
    be_ = glm::dvec3(f_coeff_ * area_ / 3.0); 

    for (int32_t i = 0; i < 3; ++i) { // Row
        for (int32_t j = 0; j < 3; ++j) { // Column
            Ke_[j][i] = a_coeff_ * (b_[i] * b_[j] + c_[i] * c_[j]) * inv4A;
            Ce_[j][i] = mfac * ((i == j) ? 2.0 : 1.0);
            Ae_[j][i] = Ke_[j][i] + Ce_[j][i];
        }
    }
}

void P1Element2D::add_robin_edge(uint32_t local_a, uint32_t local_b, double L, double k, double g) {
    const double factor = k * L / 6.0;

    Ae_[local_a][local_a] += factor * 2.0;
    Ae_[local_b][local_a] += factor; // Row local_a, Col local_b
    Ae_[local_a][local_b] += factor; // Row local_b, Col local_a
    Ae_[local_b][local_b] += factor * 2.0;

    if (g != 0.0) {
        const double fg = g * L * 0.5;
        be_[local_a] += fg;
        be_[local_b] += fg;
    }
}

void P1Element2D::add_neumann_edge(uint32_t local_a, uint32_t local_b, double L, double gN) {
    const double fg = gN * L * 0.5;
    be_[local_a] += fg;
    be_[local_b] += fg;
}

}