#pragma once

namespace fem {

class Math {
public:
    // Clamps x value in range [-1; 1] before acos
    static double safe_acos(double x);
    static bool equals(double x, double y, double epsilon = 1e-12);
};

}