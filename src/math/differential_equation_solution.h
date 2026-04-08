#ifndef FEM_DIFFERENTIAL_EQUATION_SOLUTION_H
#define FEM_DIFFERENTIAL_EQUATION_SOLUTION_H

#include <vector>

namespace fem {

template<typename Real = double>
struct DifferentialEquationSolutionT {
    Real u_min = Real(0);
    Real u_max = Real(0);
    std::vector<Real> solution_u;

    Real spectral_bilinear_energy = Real(0);

    bool is_ready() const {
        return !solution_u.empty();
    }

    void invalidate() {
        u_min = Real(0);
        u_max = Real(0);
        solution_u.clear();
        spectral_bilinear_energy = Real(0);
    }
};

using DifferentialEquationSolution  = DifferentialEquationSolutionT<double>;

}

#endif // FEM_DIFFERENTIAL_EQUATION_SOLUTION_H
