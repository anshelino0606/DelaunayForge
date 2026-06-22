#pragma once

#include "common_types_2d.h"

namespace fem {

class Geometry2D {
public:
    static double cross(const glm::dvec2& a, const glm::dvec2& b);
    static double cross(const Point2D& a, const Point2D& b);

    static bool point_in_triangle(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c);
    static bool point_in_triangle(const glm::dvec2& p, const Point2D& a, const Point2D& b, const Point2D& c);

    static double tri_area(const Point2D& a, const Point2D& b, const Point2D& c);

    static void tri_shape_coefficients(const glm::dvec2& p0, const glm::dvec2& p1, const glm::dvec2& p2, glm::dvec3& out_b, glm::dvec3& out_c);
    static void tri_shape_coefficients(const Point2D& p0, const Point2D& p1, const Point2D& p2, glm::dvec3& out_b, glm::dvec3& out_c);

    static glm::dvec2 tri_centroid(const glm::dvec2& p0, const glm::dvec2& p1, const glm::dvec2& p2);
    static glm::dvec2 tri_centroid(const Point2D& p0, const Point2D& p1, const Point2D& p2);
    static glm::dvec2 tri_centroid(double p0_x, double p0_y, double p1_x, double p1_y, double p2_x, double p2_y);
    
    static double dist2(const glm::dvec2& a, const glm::dvec2& b);
    static double point_segment_dist2(const glm::dvec2& p, const Point2D& a, const Point2D& b);

    static double angle(const glm::dvec2& u, const glm::dvec2& v);
    static double min_angle_deg(const Point2D& a, const Point2D& b, const Point2D& c);

    static double hypot(const Point2D& a, const Point2D& b);
};

}