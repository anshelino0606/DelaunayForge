#ifndef FEM_PDE_TIME_INTEGRATION_H
#define FEM_PDE_TIME_INTEGRATION_H

namespace fem {

struct NewmarkParams {
    double beta;
    double gamma;

    [[nodiscard]] consteval bool is_stable() const noexcept {
        return gamma >= 0.5 && beta >= 0.25 * (0.5 + gamma) * (0.5 + gamma);
    }

    [[nodiscard]] consteval bool is_second_order() const noexcept {
        return gamma == 0.5;
    }
};

inline constexpr NewmarkParams default_newmark{0.25, 0.5};
static_assert(default_newmark.is_stable(), "Default Newmark parameters must be stable");
static_assert(default_newmark.is_second_order(), "Default Newmark parameters should be second-order accurate");

} // namespace fem

#endif // FEM_PDE_TIME_INTEGRATION_H
