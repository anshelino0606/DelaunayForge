#ifndef DENSITY_FUNCTIONS_H
#define DENSITY_FUNCTIONS_H

#include "math_.h"
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <memory>

namespace fem {

class DensityFunction {
public:
    virtual ~DensityFunction() = default;
    
    virtual double edge_length_at(double x, double y) const = 0;
    
    virtual double density_at(double x, double y) const {
        double h = edge_length_at(x, y);
        return std::max(0.0, std::min(1.0, 1.0 / (1.0 + h)));
    }
};

class UniformDensity : public DensityFunction {
private:
    double edge_length;
public:
    UniformDensity(double h) : edge_length(h) {}
    double edge_length_at(double x, double y) const override {
        return edge_length;
    }
    double get_h() const { return edge_length; }
};

class RadialDensity : public DensityFunction {
private:
    glm::dvec2 center;
    double inner_radius;
    double outer_radius;
    double min_edge_length;
    double max_edge_length;
    
public:
    RadialDensity(glm::dvec2 c, double r_inner, double r_outer, 
                  double h_min, double h_max)
        : center(c), inner_radius(r_inner), outer_radius(r_outer),
          min_edge_length(h_min), max_edge_length(h_max) {}
    
    double edge_length_at(double x, double y) const override {
        double dist = glm::length(glm::dvec2(x, y) - center);
        
        if (dist <= inner_radius) {
            return min_edge_length;
        } else if (dist >= outer_radius) {
            return max_edge_length;
        } else {
            // Linear interpolation
            double t = (dist - inner_radius) / (outer_radius - inner_radius);
            return min_edge_length + t * (max_edge_length - min_edge_length);
        }
    }

    glm::dvec2 get_center() const { return center; }
    double get_r_inner() const { return inner_radius; }
    double get_r_outer() const { return outer_radius; }
    double get_h_min() const { return min_edge_length; }
    double get_h_max() const { return max_edge_length; }
};

class BoundaryDensity : public DensityFunction {
private:
    std::vector<glm::dvec2> boundary_points;
    double influence_distance;
    double min_edge_length;
    double max_edge_length;

public:
    BoundaryDensity(const std::vector<glm::dvec2>& boundary, double influence_dist, double h_min, double h_max)
        : boundary_points(boundary), influence_distance(influence_dist), 
          min_edge_length(h_min), max_edge_length(h_max) {}

    double edge_length_at(double x, double y) const override {
        if (boundary_points.size() < 2) return max_edge_length;

        glm::dvec2 point(x, y);
        double min_dist2_to_boundary = math::DMAX;

        const double influence2 = influence_distance * influence_distance;

        for (size_t i = 0; i < boundary_points.size(); ++i) {
            size_t next = (i + 1) % boundary_points.size();
            glm::dvec2 a = boundary_points[i];
            glm::dvec2 b = boundary_points[next];
            
            glm::dvec2 ab = b - a;
            glm::dvec2 ap = point - a;
            
            double ab_length_sq = glm::dot(ab, ab);
            if (ab_length_sq < 1e-12) {
                min_dist2_to_boundary = std::min(min_dist2_to_boundary, glm::dot(ap, ap));
                continue;
            }
            
            double t = glm::dot(ap, ab) / ab_length_sq;
            t = std::clamp(t, 0.0, 1.0);
            
            glm::dvec2 closest = a + t * ab;
            glm::dvec2 d = point - closest;
            double dist2 = glm::dot(d, d);
            min_dist2_to_boundary = std::min(min_dist2_to_boundary, dist2);

            if (min_dist2_to_boundary < 1e-24) {
                break;
            }
        }

        if (min_dist2_to_boundary <= influence2) {
            double normalized_dist = std::sqrt(std::max(0.0, min_dist2_to_boundary)) / std::max(1e-12, influence_distance);
            double smooth_t = normalized_dist * normalized_dist * (3.0 - 2.0 * normalized_dist);
        
            return min_edge_length + smooth_t * (max_edge_length - min_edge_length);
        }
        
        return max_edge_length;
    }
    
    double get_h_min() const { return min_edge_length; }
    double get_h_max() const { return max_edge_length; }
    double get_influence() const { return influence_distance; }
    const std::vector<glm::dvec2>& get_boundary_coords() const { return boundary_points; }
};

// Multi-point sources density
class MultiSourceDensity : public DensityFunction {
public:
    struct Source {
        glm::dvec2 position;
        double intensity;
        double radius;
        double min_edge_length;
        
        Source(glm::dvec2 pos, double i, double r, double h_min)
            : position(pos), intensity(i), radius(r), min_edge_length(h_min) {}
    };
    
private:
    std::vector<Source> sources;
    double base_edge_length;
    
public:
    MultiSourceDensity(double base_h) : base_edge_length(base_h) {}
    
    void add_source(glm::dvec2 pos, double intensity, double radius, double min_h) {
        sources.emplace_back(pos, intensity, radius, min_h);
    }
    
    void clear_sources() {
        sources.clear();
    }
    
    double edge_length_at(double x, double y) const override {
        glm::dvec2 point(x, y);
        double min_edge_length = base_edge_length;
        
        for (const auto& source : sources) {
            double dist = glm::length(point - source.position);
            if (dist <= source.radius) {
                double t = std::exp(-source.intensity * dist / source.radius);
                double local_h = source.min_edge_length + 
                               (base_edge_length - source.min_edge_length) * (1.0 - t);
                min_edge_length = std::min(min_edge_length, local_h);
            }
        }
        
        return min_edge_length;
    }
};

class GradientDensity : public DensityFunction {
private:
    std::function<double(double, double)> field_function;
    double gradient_threshold;
    double min_edge_length;
    double max_edge_length;
    double gradient_scale;
    
public:
    GradientDensity(std::function<double(double, double)> func,
                   double grad_threshold, double h_min, double h_max,
                   double scale = 1.0)
        : field_function(func), gradient_threshold(grad_threshold),
          min_edge_length(h_min), max_edge_length(h_max),
          gradient_scale(scale) {}
    
    double edge_length_at(double x, double y) const override {
        double h = 1e-6;
        double fx = (field_function(x + h, y) - field_function(x - h, y)) / (2 * h);
        double fy = (field_function(x, y + h) - field_function(x, y - h)) / (2 * h);
        double gradient_mag = std::sqrt(fx * fx + fy * fy) * gradient_scale;
        
        if (gradient_mag > gradient_threshold) {
            // High gradient -> small edge length
            double t = std::min(1.0, gradient_mag / (gradient_threshold * 2));
            return min_edge_length + (max_edge_length - min_edge_length) * (1.0 - t);
        }
        
        return max_edge_length;
    }
};

class CombinedDensity : public DensityFunction {
private:
    std::vector<std::unique_ptr<DensityFunction>> functions;
    
public:
    void add_function(std::unique_ptr<DensityFunction> func) {
        functions.push_back(std::move(func));
    }

    const std::vector<std::unique_ptr<DensityFunction>>& get_functions() const {
        return functions;
    }
    
    double edge_length_at(double x, double y) const override {
        if (functions.empty()) return 10.0; // Default
        
        double min_h = math::DMAX;
        for (const auto& func : functions) {
            min_h = std::min(min_h, func->edge_length_at(x, y));
        }
        return min_h;
    }
};

}

#endif // DENSITY_FUNCTIONS_H