#pragma once

#include <numbers>
#include "math/types.h"

namespace fem {

class Math {
public:
    static constexpr double pi = std::numbers::pi_v<double>;
    static constexpr double two_pi = 2.0 * pi;

    // Clamps x value in range [-1; 1] before acos
    static double safe_acos(double x);
    static bool equals(double x, double y, double epsilon = 1e-12);
};

}