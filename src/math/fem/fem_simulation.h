#ifndef FEM_SIMULATION_H
#define FEM_SIMULATION_H

#include <vector>

#include "fem_mesh.h"
#include "fem_assembler.h"
#include "fem_mesh_builder.h"
#include "geom/delaunay/delaunay_mesh_generator.h"
#include "math/differential_equation.h"
#include "math/differential_equation_solution.h"

namespace fem {

struct FEMConfig {
    int pde_preset = 1;
    double a = 1.0;
    double c = 0.0;
    double fconst = 1.0;

    int rhs_kind = 0;
    double A = 1.0;
    double kx = 0.0;
    double ky = 0.0;

    double theta_over_eps = 0.0;
    double u_c = 0.0;

    double frac_s = 0.5;
    double frac_scale = 1.0;
};

class FEMSimulation {
public:
    FEMConfig config;

    bool ready = false;
    std::vector<double> solution_u;
    double u_min = 0.0;
    double u_max = 0.0;

    FEMMesh mesh;
    fem::FEMSystem sys;

    void compute(const DelaunayTriangulationResult& R);
    void invalidate();
};

} // namespace fem

#endif // FEM_SIMULATION_H
