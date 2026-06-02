#include "planar_mesh_component.h"
#include "planar_mesh_outer_boundary.h"
#include "planar_mesh_inner_boundary.h"
#include "geom/fractal_domain_generator.h"
#include "geom/parametric_curve_generator.h"
#include "math/boundary_condition.h"
#include "math/fem/fem_mesh_builder.h"
#include "math/pde/pde_component.h"
#include "planar_mesh_generator.h"
#include "core/entity/entity.h"
#include "renderer/device.h"
#include "log_categories.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include "log_categories.h"
#include <limits>
#include <random>
#include <LLGL/Utils/VertexFormat.h>

namespace fem {

namespace {

constexpr double kLoopValidationEps = 1e-9;

struct LoopBounds {
    double xmin = std::numeric_limits<double>::max();
    double xmax = std::numeric_limits<double>::lowest();
    double ymin = std::numeric_limits<double>::max();
    double ymax = std::numeric_limits<double>::lowest();

    [[nodiscard]] bool valid() const {
        return xmin <= xmax && ymin <= ymax;
    }
};

[[nodiscard]] double orient2d(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

[[nodiscard]] int sign_eps(double value, double eps = kLoopValidationEps) {
    if (value > eps) return 1;
    if (value < -eps) return -1;
    return 0;
}

[[nodiscard]] double point_segment_distance_sq(
    const glm::dvec2& p,
    const glm::dvec2& a,
    const glm::dvec2& b
) {
    const glm::dvec2 ab = b - a;
    const double denom = glm::dot(ab, ab);
    if (denom <= kLoopValidationEps) {
        return glm::dot(p - a, p - a);
    }

    const double t = std::clamp(glm::dot(p - a, ab) / denom, 0.0, 1.0);
    const glm::dvec2 q = a + t * ab;
    return glm::dot(p - q, p - q);
}

[[nodiscard]] bool point_on_segment(
    const glm::dvec2& p,
    const glm::dvec2& a,
    const glm::dvec2& b,
    double eps = kLoopValidationEps
) {
    if (std::abs(orient2d(a, b, p)) > eps) return false;

    const double xmin = std::min(a.x, b.x) - eps;
    const double xmax = std::max(a.x, b.x) + eps;
    const double ymin = std::min(a.y, b.y) - eps;
    const double ymax = std::max(a.y, b.y) + eps;
    return p.x >= xmin && p.x <= xmax && p.y >= ymin && p.y <= ymax;
}

[[nodiscard]] bool segments_intersect_or_touch(
    const glm::dvec2& a,
    const glm::dvec2& b,
    const glm::dvec2& c,
    const glm::dvec2& d,
    double eps = kLoopValidationEps
) {
    const double o1 = orient2d(a, b, c);
    const double o2 = orient2d(a, b, d);
    const double o3 = orient2d(c, d, a);
    const double o4 = orient2d(c, d, b);

    const int s1 = sign_eps(o1, eps);
    const int s2 = sign_eps(o2, eps);
    const int s3 = sign_eps(o3, eps);
    const int s4 = sign_eps(o4, eps);

    if (s1 * s2 < 0 && s3 * s4 < 0) return true;
    if (s1 == 0 && point_on_segment(c, a, b, eps)) return true;
    if (s2 == 0 && point_on_segment(d, a, b, eps)) return true;
    if (s3 == 0 && point_on_segment(a, c, d, eps)) return true;
    if (s4 == 0 && point_on_segment(b, c, d, eps)) return true;
    return false;
}

[[nodiscard]] double segment_segment_distance_sq(
    const glm::dvec2& a,
    const glm::dvec2& b,
    const glm::dvec2& c,
    const glm::dvec2& d
) {
    if (segments_intersect_or_touch(a, b, c, d)) {
        return 0.0;
    }

    return std::min({
        point_segment_distance_sq(a, c, d),
        point_segment_distance_sq(b, c, d),
        point_segment_distance_sq(c, a, b),
        point_segment_distance_sq(d, a, b),
    });
}

[[nodiscard]] bool point_in_poly(const std::vector<Point2D>& poly, double x, double y) {
    if (poly.size() < 3) return false;

    bool inside = false;
    std::size_t j = poly.size() - 1;
    for (std::size_t i = 0; i < poly.size(); j = i++) {
        const Point2D& a = poly[i];
        const Point2D& b = poly[j];
        const bool cond = (a.y() > y) != (b.y() > y);
        if (!cond) continue;

        const double denom = (b.y() - a.y()) + 1e-300;
        const double x_on_edge = (b.x() - a.x()) * (y - a.y()) / denom + a.x();
        if (x < x_on_edge) inside = !inside;
    }
    return inside;
}

[[nodiscard]] double loop_signed_area(const std::vector<Point2D>& loop) {
    if (loop.size() < 3) return 0.0;

    double area = 0.0;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const Point2D& p = loop[i];
        const Point2D& q = loop[(i + 1) % loop.size()];
        area += p.x() * q.y() - q.x() * p.y();
    }
    return 0.5 * area;
}

void normalize_boundary_loop(std::vector<Point2D>& loop, bool make_clockwise) {
    if (make_clockwise ? (loop_signed_area(loop) > 0.0) : (loop_signed_area(loop) < 0.0)) {
        std::reverse(loop.begin(), loop.end());
    }

    for (std::size_t i = 0; i < loop.size(); ++i) {
        loop[i].id = static_cast<int>(i);
        loop[i].on_boundary = true;
    }
}

[[nodiscard]] LoopBounds compute_loop_bounds(const std::vector<Point2D>& loop) {
    LoopBounds bounds;
    for (const Point2D& point : loop) {
        bounds.xmin = std::min(bounds.xmin, point.x());
        bounds.xmax = std::max(bounds.xmax, point.x());
        bounds.ymin = std::min(bounds.ymin, point.y());
        bounds.ymax = std::max(bounds.ymax, point.y());
    }
    return bounds;
}

[[nodiscard]] bool loop_self_intersects(const std::vector<Point2D>& loop) {
    if (loop.size() < 4) return false;

    for (std::size_t i = 0; i < loop.size(); ++i) {
        const glm::dvec2 a = loop[i].p;
        const glm::dvec2 b = loop[(i + 1) % loop.size()].p;

        for (std::size_t j = i + 1; j < loop.size(); ++j) {
            const std::size_t i_next = (i + 1) % loop.size();
            const std::size_t j_next = (j + 1) % loop.size();

            if (i == j || i == j_next || i_next == j || i_next == j_next) {
                continue;
            }

            const glm::dvec2 c = loop[j].p;
            const glm::dvec2 d = loop[j_next].p;
            if (segments_intersect_or_touch(a, b, c, d)) {
                return true;
            }
        }
    }

    return false;
}

[[nodiscard]] bool point_has_clearance_from_loop(
    const glm::dvec2& point,
    const std::vector<Point2D>& loop,
    double clearance_sq
) {
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const glm::dvec2 a = loop[i].p;
        const glm::dvec2 b = loop[(i + 1) % loop.size()].p;
        if (point_segment_distance_sq(point, a, b) <= clearance_sq) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool loop_has_clearance_from_loop(
    const std::vector<Point2D>& candidate,
    const std::vector<Point2D>& obstacle,
    double clearance_sq
) {
    for (std::size_t i = 0; i < candidate.size(); ++i) {
        const glm::dvec2 a = candidate[i].p;
        const glm::dvec2 b = candidate[(i + 1) % candidate.size()].p;
        for (std::size_t j = 0; j < obstacle.size(); ++j) {
            const glm::dvec2 c = obstacle[j].p;
            const glm::dvec2 d = obstacle[(j + 1) % obstacle.size()].p;
            if (segment_segment_distance_sq(a, b, c, d) <= clearance_sq) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] std::optional<std::string> generate_inner_boundary_template(
    const RandomInnerBoundaryConfig& cfg,
    const glm::dvec2& center,
    std::vector<Point2D>& out_loop
) {
    out_loop.clear();

    if (cfg.source_type == RandomInnerBoundarySourceType::Parametric) {
        ParametricCurveConfig parametric_cfg = cfg.parametric_template;
        parametric_cfg.center = center;
        if (auto err = ParametricCurveGenerator::generate(parametric_cfg, out_loop)) {
            return err;
        }
    } else {
        FractalDomainConfig fractal_cfg = cfg.fractal_template;
        fractal_cfg.center = center;

        switch (fractal_cfg.preset.value) {
            case FractalPreset::KochSnowflake:
            case FractalPreset::QuadraticKochIsland:
            case FractalPreset::MinkowskiIsland:
            case FractalPreset::MidpointDisplacementLoop:
                break;
            default:
                return "selected fractal preset cannot be used as a single random inner boundary";
        }

        if (auto err = FractalDomainGenerator::generate(fractal_cfg, out_loop)) {
            return err;
        }
    }

    normalize_boundary_loop(out_loop, true);

    if (loop_self_intersects(out_loop)) {
        return "random inner boundary template self-intersects";
    }

    return std::nullopt;
}

[[nodiscard]] bool loop_is_valid_inner_boundary(
    const std::vector<Point2D>& candidate,
    const std::vector<Point2D>& outer,
    const std::vector<std::vector<Point2D>>& occupied_holes,
    double min_clearance
) {
    if (candidate.size() < 3 || outer.size() < 3) return false;
    if (loop_self_intersects(candidate)) return false;

    const double clearance_sq = min_clearance * min_clearance;

    for (const Point2D& point : candidate) {
        if (!point_in_poly(outer, point.x(), point.y())) {
            return false;
        }
        if (!point_has_clearance_from_loop(point.p, outer, clearance_sq)) {
            return false;
        }

        for (const auto& hole : occupied_holes) {
            if (point_in_poly(hole, point.x(), point.y())) {
                return false;
            }
            if (!point_has_clearance_from_loop(point.p, hole, clearance_sq)) {
                return false;
            }
        }
    }

    if (!loop_has_clearance_from_loop(candidate, outer, clearance_sq)) {
        return false;
    }

    for (const auto& hole : occupied_holes) {
        if (!loop_has_clearance_from_loop(candidate, hole, clearance_sq)) {
            return false;
        }
        if (!hole.empty() && point_in_poly(candidate, hole.front().x(), hole.front().y())) {
            return false;
        }
    }

    return true;
}

} // namespace

FEM_DEFINE_OBJECT(PlanarMeshComponent, MeshComponent);

FEM_BEGIN_PROPERTY_REGISTER(PlanarMeshComponent)
{
    FEM_REGISTER_PROPERTY(PlanarMeshComponent, mesh_generator_, NoTypeHeader(), Transient());
    FEM_REGISTER_PROPERTY(PlanarMeshComponent, outer_boundary_);
    
    FEM_REGISTER_PROPERTY(
        PlanarMeshComponent, 
        inner_boundaries_,
        ON_ARRAY_VALUE_ADDED(PlanarMeshComponent, on_inner_boundary_added),
        ON_ARRAY_VALUE_REMOVED(
            PlanarMeshComponent, 
            on_inner_boundary_pre_removed,
            on_inner_boundary_post_removed
        ) 
    );

    FEM_REGISTER_PROPERTY(
        PlanarMeshComponent, 
        boundary_conditions_,
        ON_ARRAY_VALUE_PRE_REMOVED(
            PlanarMeshComponent, 
            on_boundary_condition_pre_removed
        )
    );

    FEM_REGISTER_PROPERTY(
        PlanarMeshComponent, 
        density_config_, 
        ON_VALUE_CHANGED(PlanarMeshComponent, triangulate)
    );

    FEM_REGISTER_PROPERTY(
        PlanarMeshComponent,
        random_inner_boundary_config_,
        DisplayName("Random Inner Boundaries"),
        NoTypeHeader(),
        Transient()
    );

    FEM_REGISTER_FUNCTION(PlanarMeshComponent, triangulate);
    FEM_REGISTER_FUNCTION(
        PlanarMeshComponent,
        add_random_inner_boundaries,
        DisplayName("Add Random Inner Boundaries")
    );
    
    FEM_REGISTER_FUNCTION(
        PlanarMeshComponent, 
        enable_free_hand, 
        SameLine(),
        SHOW_WHEN_MEMBER(PlanarMeshComponent, is_free_hand_enabled_, !val)
    );

    FEM_REGISTER_FUNCTION(
        PlanarMeshComponent,
        disable_free_hand,
        SameLine(),
        SHOW_WHEN_MEMBER(PlanarMeshComponent, is_free_hand_enabled_, val)
    );

    FEM_REGISTER_FUNCTION(PlanarMeshComponent, clear_boundary_conditions);
    FEM_REGISTER_FUNCTION(PlanarMeshComponent, clear_outer_boundary, SameLine());
    FEM_REGISTER_FUNCTION(PlanarMeshComponent, clear_inner_boundaries, SameLine());
    FEM_REGISTER_FUNCTION(PlanarMeshComponent, reset, DisplayName("Clear All"), SameLine());

    FEM_REGISTER_PROPERTY(PlanarMeshComponent, user_points_, NoUI());
    FEM_REGISTER_PROPERTY(PlanarMeshComponent, boundary_points_, NoUI());
    FEM_REGISTER_PROPERTY(PlanarMeshComponent, polygon_points_, NoUI());
    FEM_REGISTER_PROPERTY(PlanarMeshComponent, boundary_loops_, NoUI());
    FEM_REGISTER_PROPERTY(PlanarMeshComponent, triangulation_result_, NoUI());
}
FEM_END_PROPERTY_REGISTER(PlanarMeshComponent);

FEM_DEFINE_STRUCT(BoundaryLoopContainer);
FEM_BEGIN_PROPERTY_REGISTER(BoundaryLoopContainer)
{
    FEM_REGISTER_PROPERTY(BoundaryLoopContainer, points);
}
FEM_END_PROPERTY_REGISTER(BoundaryLoopContainer)

PlanarMeshComponent::PlanarMeshComponent() {
    outer_boundary_ = create_subobject<PlanarMeshOuterBoundary>();
    mesh_generator_ = PlanarMeshGenerator::default_generator();
}

PlanarMeshComponent::~PlanarMeshComponent() {
    destroy_object(outer_boundary_);

    for (PlanarMeshInnerBoundary* boundary : inner_boundaries_) {
        destroy_object(boundary);
    }

    if (vertex_buffer_) 
        g_device->Release(*vertex_buffer_);

    if (index_buffer_)
        g_device->Release(*index_buffer_);
}

void PlanarMeshComponent::add_user_point(glm::dvec2 pos) {
    user_points_.emplace_back(pos.x, pos.y, (int)user_points_.size());
}

void PlanarMeshComponent::add_polygon_point(glm::dvec2 pos) {
    polygon_points_.emplace_back(pos.x, pos.y, (int)polygon_points_.size());
}

void PlanarMeshComponent::add_boundary_point(glm::dvec2 pos) {
    boundary_points_.emplace_back(pos.x, pos.y, (int)boundary_points_.size());
}

void PlanarMeshComponent::add_inner_boundary(std::vector<Point2D>& points) {
    PlanarMeshInnerBoundary* inner_boundary = create_subobject<PlanarMeshInnerBoundary>();
    inner_boundaries_.push_back(inner_boundary);
    inner_boundary->set_points(points);
}

void PlanarMeshComponent::set_outer_boundary(std::vector<Point2D>& points) {
    outer_boundary_->set_points(points);
}

void PlanarMeshComponent::reset() {
    user_points_.clear();
    polygon_points_.clear();
    boundary_points_.clear();
    boundary_loops_.clear();

    triangulation_result_ = {};

    outer_boundary_->clear_points();
    for (PlanarMeshInnerBoundary* inner_boundary : inner_boundaries_) {
        inner_boundary->clear_points();
    }
}

void PlanarMeshComponent::set_edited_boundary(PlanarMeshBoundaryBase* boundary) {
    if (edited_boundary_) {
        edited_boundary_->set_is_editing_enabled(false);
    }

    edited_boundary_ = boundary;
}

void PlanarMeshComponent::reset_edited_boundary() {
    set_edited_boundary(nullptr);
}

void PlanarMeshComponent::set_edited_boundary_condition(BoundaryCondition* boundary_condition) {
    edited_boundary_condition_ = boundary_condition;
}

void PlanarMeshComponent::triangulate() {
    const auto t0 = std::chrono::steady_clock::now();

    if (outer_boundary_ && !outer_boundary_->points().empty()) {
        set_single_boundary_loop(outer_boundary_->points(), true);
    }
    
    // Sync inner boundaries to boundary_loops_
    for (size_t i = 0; i < inner_boundaries_.size(); ++i) {
        if (inner_boundaries_[i] && !inner_boundaries_[i]->points().empty()) {
            add_boundary_loop(inner_boundaries_[i]->points(), true);
        }
    }
    mesh_generator_->triangulate(this);

    update_buffers();
    
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    if (ms > 50.0) {
        LOGT_DEBUG(LogGeometry,
                  "Triangulation took %.2f ms (user_pts=%zu, loops=%zu, inner=%zu, density=%s)",
                  ms,
                  user_points_.size(),
                  boundary_loops_.size(),
                  inner_boundaries_.size(),
                  (density_config_.enable ? "on" : "off"));
    }
}

void PlanarMeshComponent::request_triangulate() {
    if (bulk_update_depth_ > 0) {
        triangulation_dirty_ = true;
        return;
    }
    triangulate();
}

void PlanarMeshComponent::update_triangulation(const DelaunayTriangulationResult& new_result) {
    for (BoundaryCondition* bc : boundary_conditions_) {
        if (!bc) continue;
        bc->capture_geometry_from_edges(triangulation_result_);
        bc->capture_parameterization_from_edges(triangulation_result_);
    }

    triangulation_result_ = new_result;

    int total_remapped = 0;
    int total_failed = 0;

    for (BoundaryCondition* bc : boundary_conditions_) {
        if (!bc) continue;

        int old_count = (int)bc->edge_ids().size();
        bc->remap_after_retriangulation();
        int new_count = (int)bc->edge_ids().size();
        
        if (old_count > 0) {
            if (new_count > 0) {
                total_remapped++;
            } else {
                total_failed++;
            }
        }
    }

    if (total_remapped > 0 || total_failed > 0) {
        LOGT_DEBUG(LogMath,
                  "Boundary conditions remapped after retriangulation: success=%d, failed=%d",
                  total_remapped, total_failed);
    }
}

FEMMesh PlanarMeshComponent::build_fem_mesh() const {
    for (BoundaryCondition* bc : boundary_conditions_) {
        if (!bc || bc->type() == BoundaryConditionType::None) continue;

        for (int edge_id : bc->edge_ids()) {
            EdgeInfo& edge = triangulation_result_.edges[edge_id];

            switch (bc->type()) {
                case BoundaryConditionType::Dirichlet:
                    edge.bc.type       = fem::BCType::Dirichlet;
                    edge.bc.value      = bc->value();       // uD
                    edge.bc.value_beta = 0.0;
                    break;

                case BoundaryConditionType::Neumann:
                    edge.bc.type       = fem::BCType::Neumann;
                    edge.bc.value      = bc->value();       // gN
                    edge.bc.value_beta = 0.0;
                    break;

                case BoundaryConditionType::Robin:
                    edge.bc.type       = fem::BCType::Robin;
                    edge.bc.value      = bc->robin_alpha(); // k
                    edge.bc.value_beta = bc->robin_beta();  // g
                    break;

                default:
                    edge.bc = fem::BoundaryValue::none();
                    break;
            }
        }
    }
    
    FEMMesh fem_mesh = ::fem::build_fem_mesh(triangulation_result_);

    // Reset boundary conditions in triangulation_result
    for (BoundaryCondition* boundary_condition : boundary_conditions_) {
        for (int edge_id : boundary_condition->edge_ids()) {
            EdgeInfo& edge = triangulation_result_.edges[edge_id];
            edge.bc.type = fem::BCType::None;   // TEMP!!!!!
            edge.bc.value = 0.0;
            edge.bc.value_beta = 0.0;
        }
    }

    return fem_mesh;
}

void PlanarMeshComponent::update_buffers() {
    if (triangulation_result_.points.size() < 3)
        return;

    PDEComponent* pde = entity_->get_component<PDEComponent>();

    if (!pde) {
        LOGT_ERROR(LogGeometry, "PlanarMeshComponent::update_buffers(): Failed to find PDEComponent. 3D mesh can't be created!");
        return;
    }

    uint64_t new_vertex_buffer_size = sizeof(glm::vec3) * triangulation_result_.points.size();
    uint64_t new_index_buffer_size = sizeof(uint32_t) * triangulation_result_.triangles.size() * 3;

    const bool needs_vertex_buffer = !vertex_buffer_ || vertex_buffer_->GetDesc().size < new_vertex_buffer_size;
    const bool needs_index_buffer = !index_buffer_ || index_buffer_->GetDesc().size < new_index_buffer_size;

    if (needs_vertex_buffer || needs_index_buffer) {
        if (vertex_buffer_) {
            g_device->Release(*vertex_buffer_);
            vertex_buffer_ = nullptr;
        }

        if (index_buffer_) {
            g_device->Release(*index_buffer_);
            index_buffer_ = nullptr;
        }

        LLGL::VertexFormat vertex_format;
        vertex_format.attributes = {
            LLGL::VertexAttribute{"position", LLGL::Format::RGB32Float, 0, 0, sizeof(glm::vec3), 0}
        };

        LLGL::BufferDescriptor vertex_buffer_desc;
        vertex_buffer_desc.size = new_vertex_buffer_size;
        vertex_buffer_desc.bindFlags = LLGL::BindFlags::VertexBuffer;
        vertex_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
        vertex_buffer_desc.vertexAttribs = vertex_format.attributes;
        
        vertex_buffer_ = g_device->CreateBuffer(vertex_buffer_desc);

        LLGL::BufferDescriptor index_buffer_desc;
        index_buffer_desc.size = new_index_buffer_size;
        index_buffer_desc.bindFlags = LLGL::BindFlags::IndexBuffer;
        index_buffer_desc.cpuAccessFlags = LLGL::CPUAccessFlags::Write;
        index_buffer_desc.format = LLGL::Format::R32UInt;

        index_buffer_ = g_device->CreateBuffer(index_buffer_desc);
    }

    const DifferentialEquationSolution& solution = pde->solution(this);
    bool can_use_solution_as_height = solution.solution_u.size() == triangulation_result_.points.size();

    std::vector<glm::vec3> vertices;
    vertices.reserve(triangulation_result_.points.size());

    constexpr double display_max_height = 100.0;
    constexpr double display_min_height = -100.0;

    auto [global_u_min, global_u_max] = pde->get_global_bounds();
    if (global_u_min > global_u_max) {
        global_u_min = solution.u_min;
        global_u_max = solution.u_max;
    }

    double height_scale = 1.0;
    if (can_use_solution_as_height) {
        if (global_u_max != 0.0 && global_u_max > display_max_height) {
            height_scale = std::min(height_scale, display_max_height / global_u_max);
        }
        if (global_u_min != 0.0 && global_u_min < display_min_height) {
            height_scale = std::min(height_scale, display_min_height / global_u_min);
        }

        const float display_u_min = (float)std::clamp(global_u_min * height_scale, display_min_height, display_max_height);
        const float display_u_max = (float)std::clamp(global_u_max * height_scale, display_min_height, display_max_height);
        set_display_u_bounds(display_u_min, display_u_max);
    } else {
        clear_display_u_bounds();
    }

    for (size_t i = 0; i != triangulation_result_.points.size(); ++i) {
        const Point2D& point = triangulation_result_.points[i];

        glm::vec3& vertex = vertices.emplace_back();
        vertex.x = point.x();
        vertex.y = 0;
        vertex.z = point.y();

        if (can_use_solution_as_height) {
            const double solution_value = solution.solution_u[i];
            const double scaled_value = solution_value * height_scale;
            vertex.y = std::clamp(scaled_value, display_min_height, display_max_height);
        }
    }

    g_device->WriteBuffer(*vertex_buffer_, 0, vertices.data(), vertices.size() * sizeof(glm::vec3));

    std::vector<uint32_t> indices;
    indices.reserve(triangulation_result_.triangles.size() * 3);

    for (const Tri& tri : triangulation_result_.triangles) {
        indices.push_back(tri.v.x);
        indices.push_back(tri.v.y);
        indices.push_back(tri.v.z);
    }

    g_device->WriteBuffer(*index_buffer_, 0, indices.data(), indices.size() * sizeof(uint32_t));

}

uint32_t PlanarMeshComponent::index_count() const {
    return triangulation_result_.triangles.size() * 3;
}

void PlanarMeshComponent::normalize_boundary_pts_(std::vector<Point2D>& pts, bool mark_boundary) {
    for (size_t i = 0; i < pts.size(); ++i) {
        pts[i].id = (int)i;
        if (mark_boundary) pts[i].on_boundary = true;
    }
}

void PlanarMeshComponent::set_polygon_points(std::vector<Point2D> pts, bool mark_boundary) {
    normalize_boundary_pts_(pts, mark_boundary);
    polygon_points_ = std::move(pts);
}

void PlanarMeshComponent::set_boundary_points(std::vector<Point2D> pts, bool mark_boundary) {
    normalize_boundary_pts_(pts, mark_boundary);
    boundary_points_ = std::move(pts);
}

void PlanarMeshComponent::add_boundary_loop(std::vector<Point2D> loop, bool mark_boundary) {
    normalize_boundary_pts_(loop, mark_boundary);
    BoundaryLoopContainer& container = boundary_loops_.emplace_back();
    container.points = std::move(loop);
}

std::shared_ptr<DensityFunction> PlanarMeshComponent::build_density_function() const {
    auto density_combo = std::make_shared<CombinedDensity>();
    density_combo->add_function(std::make_unique<UniformDensity>(density_config_.global_h));
    
    if (density_config_.use_boundary) {
        auto add_boundary_density = [&](const std::vector<Point2D>& points) {
            if (points.size() < 2) return;
            
            std::vector<glm::dvec2> coords;
            coords.reserve(points.size());
            for (const auto& p : points) {
                coords.emplace_back(p.x(), p.y());
            }
            
            density_combo->add_function(std::make_unique<BoundaryDensity>(
                std::move(coords),  // Move instead of copy
                density_config_.boundary_influence,
                density_config_.boundary_h_min,
                density_config_.boundary_h_max
            ));
        };
        
        if (!boundary_loops_.empty()) {
            for (const auto& loop : boundary_loops_) {
                add_boundary_density(loop.points);
            }
        }
        else if (!polygon_points_.empty()) {
            add_boundary_density(polygon_points_);
        }
        else if (!boundary_points_.empty()) {
            add_boundary_density(boundary_points_);
        }
    }
    
    if (density_config_.use_radial) {
        density_combo->add_function(std::make_unique<RadialDensity>(
            density_config_.radial_center,
            density_config_.radial_r_in,
            density_config_.radial_r_out,
            density_config_.radial_h_min,
            density_config_.radial_h_max
        ));
    }
    
    return density_combo;
}


void PlanarMeshComponent::clear_boundary_conditions() {
    boundary_conditions_.clear();
}

void PlanarMeshComponent::clear_outer_boundary() {
    outer_boundary_->clear_points();
    triangulation_result_ = {};
}

void PlanarMeshComponent::clear_inner_boundaries() {
    for (PlanarMeshInnerBoundary* boundary : inner_boundaries_) {
        if (!boundary) continue;
        destroy_object(boundary);
    }
    inner_boundaries_.clear();
    request_triangulate();
}

void PlanarMeshComponent::begin_bulk_update() {
    bulk_update_depth_++;
}

void PlanarMeshComponent::end_bulk_update(bool triangulate_after) {
    bulk_update_depth_ = std::max(0, bulk_update_depth_ - 1);
    if (bulk_update_depth_ == 0 && triangulation_dirty_) {
        triangulation_dirty_ = false;
        if (triangulate_after) {
            triangulate();
        }
    }
}

void PlanarMeshComponent::replace_inner_boundaries(std::vector<std::vector<Point2D>> loops) {
    const bool was_bulk = is_in_bulk_update();
    if (!was_bulk) {
        begin_bulk_update();
    }

    clear_inner_boundaries();

    for (auto& loop : loops) {
        if (loop.empty()) continue;
        PlanarMeshInnerBoundary* inner_boundary = create_subobject<PlanarMeshInnerBoundary>();
        inner_boundaries_.push_back(inner_boundary);
        inner_boundary->set_points(loop);
        inner_boundary->disable_editing();
    }

    if (!was_bulk) {
        end_bulk_update(true);
    }
}

void PlanarMeshComponent::add_random_inner_boundaries() {
    if (!outer_boundary_ || outer_boundary_->points().size() < 3) {
        LOGT_ERROR(LogGeometry, "Random inner boundaries require a valid outer boundary first");
        return;
    }

    const int requested_count = std::max(0, random_inner_boundary_config_.count);
    if (requested_count == 0) {
        LOGT_WARN(LogGeometry, "Random inner boundaries requested with count=0");
        return;
    }

    const LoopBounds bounds = compute_loop_bounds(outer_boundary_->points());
    if (!bounds.valid()) {
        LOGT_ERROR(LogGeometry, "Failed to compute outer boundary bounds for random inner boundaries");
        return;
    }

    const double min_clearance = std::max(0.0, random_inner_boundary_config_.min_clearance);
    const double clearance_sq = min_clearance * min_clearance;

    std::mt19937 rng(random_inner_boundary_config_.seed);
    std::uniform_real_distribution<double> xdist(bounds.xmin, bounds.xmax);
    std::uniform_real_distribution<double> ydist(bounds.ymin, bounds.ymax);

    std::vector<std::vector<Point2D>> occupied_holes;
    occupied_holes.reserve(inner_boundaries_.size() + static_cast<std::size_t>(requested_count));
    if (!random_inner_boundary_config_.replace_existing) {
        for (PlanarMeshInnerBoundary* inner_boundary : inner_boundaries_) {
            if (!inner_boundary || inner_boundary->points().empty()) continue;
            occupied_holes.push_back(inner_boundary->points());
        }
    }

    std::vector<std::vector<Point2D>> generated_loops;
    generated_loops.reserve(static_cast<std::size_t>(requested_count));

    int total_attempts = 0;
    for (int idx = 0; idx < requested_count; ++idx) {
        bool placed = false;

        for (int attempt = 0; attempt < random_inner_boundary_config_.max_attempts_per_boundary; ++attempt) {
            ++total_attempts;

            const glm::dvec2 center{xdist(rng), ydist(rng)};
            if (!point_in_poly(outer_boundary_->points(), center.x, center.y)) {
                continue;
            }
            if (!point_has_clearance_from_loop(center, outer_boundary_->points(), clearance_sq)) {
                continue;
            }

            bool center_is_blocked = false;
            for (const auto& hole : occupied_holes) {
                if (point_in_poly(hole, center.x, center.y) || !point_has_clearance_from_loop(center, hole, clearance_sq)) {
                    center_is_blocked = true;
                    break;
                }
            }
            if (center_is_blocked) {
                continue;
            }

            std::vector<Point2D> candidate;
            if (auto err = generate_inner_boundary_template(random_inner_boundary_config_, center, candidate)) {
                LOGT_ERROR(LogGeometry, "Random inner boundary generation failed: %s", err->c_str());
                return;
            }

            if (!loop_is_valid_inner_boundary(candidate, outer_boundary_->points(), occupied_holes, min_clearance)) {
                continue;
            }

            occupied_holes.push_back(candidate);
            generated_loops.push_back(std::move(candidate));
            placed = true;
            break;
        }

        if (!placed) {
            break;
        }
    }

    if (generated_loops.empty()) {
        LOGT_WARN(
            LogGeometry,
            "Failed to place any random inner boundaries after %d attempts",
            total_attempts
        );
        return;
    }

    const bool was_bulk = is_in_bulk_update();
    if (!was_bulk) {
        begin_bulk_update();
    }

    if (random_inner_boundary_config_.replace_existing) {
        clear_inner_boundaries();
    }

    for (auto& loop : generated_loops) {
        PlanarMeshInnerBoundary* inner_boundary = create_subobject<PlanarMeshInnerBoundary>();
        inner_boundaries_.push_back(inner_boundary);
        inner_boundary->set_points(loop);
        inner_boundary->disable_editing();
    }

    if (!was_bulk) {
        end_bulk_update(true);
    }

    if ((int)generated_loops.size() < requested_count) {
        LOGT_WARN(
            LogGeometry,
            "Placed %zu/%d random inner boundaries after %d attempts",
            generated_loops.size(),
            requested_count,
            total_attempts
        );
        return;
    }

    LOGT_INFO(
        LogGeometry,
        "Placed %zu random inner boundaries after %d attempts",
        generated_loops.size(),
        total_attempts
    );
}

void PlanarMeshComponent::on_inner_boundary_added() {
    if (edited_boundary_) {
        edited_boundary_->disable_editing();
    }

    inner_boundaries_.back()->enable_editing();
}

void PlanarMeshComponent::on_inner_boundary_pre_removed(void* elem) {
    PlanarMeshInnerBoundary* inner = static_cast<PlanarMeshInnerBoundary*>(elem);
    if (inner == edited_boundary_) {
        inner->disable_editing();
    }
}

void PlanarMeshComponent::on_inner_boundary_post_removed() {
    triangulate();
}

void PlanarMeshComponent::on_boundary_condition_pre_removed(void* elem) {
    BoundaryCondition* bc = static_cast<BoundaryCondition*>(elem);
    if (bc == edited_boundary_condition_) {
        edited_boundary_condition_ = nullptr;
    }
}

void PlanarMeshComponent::enable_free_hand() {
    if (edited_boundary_) {
        edited_boundary_->disable_editing();
    }

    is_free_hand_enabled_ = true;
}

void PlanarMeshComponent::disable_free_hand() {
    is_free_hand_enabled_ = false;
}

}