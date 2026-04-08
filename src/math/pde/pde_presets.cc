#include "pde_presets.h"
#include "math/fem/fem_assemblers_p1.h"
#include "parameters/pde_fractional_operator.h"
#include <cmath>

namespace fem {

FEM_DEFINE_PDE_PRESET(
    PDEPreset_Laplace,
    "Laplace (-u = 0)",
    {
        .fem_assembler = assemble_and_solve_local_P1
    }
);

FEM_DEFINE_PDE_PRESET(
    PDEPreset_Poisson,
    "Poisson (-u = f)",
    {
        .fem_assembler = assemble_and_solve_local_P1
    }
);

template<>
bool evaluate_exact_solution<PDEPreset_Poisson>(double x, double y, double& u_exact,
                                               double* ux_exact, double* uy_exact) {
    const double pi = M_PI;
    u_exact = std::sin(pi * x) * std::sin(pi * y);
    
    if (ux_exact) {
        *ux_exact = pi * std::cos(pi * x) * std::sin(pi * y);
    }
    if (uy_exact) {
        *uy_exact = pi * std::sin(pi * x) * std::cos(pi * y);
    }
    
    return true;
}

FEM_DEFINE_PDE_PRESET(
    PDEPreset_Reaction,
    "Reaction (-u? + c u = f)",
    {
        .fem_assembler = assemble_and_solve_local_P1
    }
);

FEM_DEFINE_PDE_PRESET(
    PDEPreset_Fractional,
    "Fractional ((-delta)^s u = f)",
    {
        .fem_assembler = assemble_and_solve_fractional_auto_P1
    },
    FractionalOperator
);

FEM_DEFINE_PDE_PRESET(
    PDEPreset_Heat,
    "Heat (u_t - div(a grad u) + c u = f)",
    {
        .fem_assembler = assemble_and_solve_heat_implicit_euler_P1
    }
);

FEM_DEFINE_PDE_PRESET(
    PDEPreset_Wave,
    "Wave (u_tt - div(a grad u) + c u = f)",
    {
        .fem_assembler = assemble_and_solve_wave_newmark_P1
    }
);

}