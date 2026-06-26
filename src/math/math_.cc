#include "math_.h"
#include <cmath>
#include <algorithm>

namespace fem::math {

double safe_acos(double x) {
    return std::acos(std::clamp(x, -1.0, 1.0));
}

bool equals(double x, double y, double epsilon) {
    return std::abs(x - y) < epsilon;
}

int32_t sign_eps(double value, double epsilon) {
    if (value > epsilon) return 1;
    if (value < -epsilon) return -1;
    return 0;
}

}