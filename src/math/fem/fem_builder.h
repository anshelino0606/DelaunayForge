#ifndef FEM_BUILDER
#define FEM_BUILDER

#include <functional>
#include "fem_problem.h"
#include "fem_assembler.h"

namespace fem {
using FEMBuilder = std::function<FEMSystem(const FEMProblem&)>;
}

#endif
