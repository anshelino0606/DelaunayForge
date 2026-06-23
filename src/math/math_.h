#pragma once

#include <limits>
#include <numbers>

namespace fem {

class Math {
public:
    static constexpr double DMIN = std::numeric_limits<double>::lowest();
    static constexpr double DMAX = std::numeric_limits<double>::max();
    static constexpr double DNAN = std::numeric_limits<double>::quiet_NaN();
    static constexpr double DINF = std::numeric_limits<double>::infinity();

    static constexpr double PI = std::numbers::pi;
    static constexpr float F_PI = std::numbers::pi_v<float>;
    static constexpr double E = std::numbers::e;

    // Clamps x value in range [-1; 1] before acos
    static double safe_acos(double x);
    static bool equals(double x, double y, double epsilon = 1e-12);
};

}