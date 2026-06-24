#include "solve_request.h"

namespace fem {

bool is_transient_solve(SolveKind kind) noexcept {
    return kind == SolveKind::HeatImplicitEuler || kind == SolveKind::WaveNewmark;
}

} // namespace fem
