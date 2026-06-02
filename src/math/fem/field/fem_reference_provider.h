#pragma once
#include <optional>
#include <functional>
#include "math/fem/fem_mesh.h"
#include "math/fem/fem_assembler.h"
#include "math/differential_equation_solution.h"
#include "math/fem/field/fem_reference_provider.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "math/fem/fem_simulation.h"
#include "math/fem/fem_mesh_builder.h"
#include "log_categories.h"
#include <string>

namespace fem {
class PDEComponent;

DelaunayTriangulationResult refine_delaunay_uniform(
        const DelaunayTriangulationResult& input);

enum class ReferenceRefinementStrategy {
    UniformFemSubdivision,
    UniformTriangulationSubdivision,
};
    
struct ReferenceSolveRequest {
    int refinement_level = 1;
    ReferenceRefinementStrategy refinement_strategy =
        ReferenceRefinementStrategy::UniformFemSubdivision;
};

struct ReferenceSolution {
    DelaunayTriangulationResult triangulation;
    bool has_triangulation = false;
    FEMMesh mesh;
    DifferentialEquationSolution sol;
    FEMSystem sys;
    bool has_sys = false;
    std::string error_message;
};

class IReferenceProvider {
public:
    virtual ~IReferenceProvider() = default;
    virtual bool solve_reference(const ReferenceSolveRequest& req, 
                            ReferenceSolution& out) const = 0;
};


class FEMReferenceProvider final : public IReferenceProvider {
    const PDEComponent* pde_ = nullptr;
    const PlanarMeshComponent* mesh_ = nullptr;

public:
    FEMReferenceProvider(const PDEComponent* pde, const PlanarMeshComponent* mesh)
        : pde_(pde), mesh_(mesh) {}

    bool solve_reference(const ReferenceSolveRequest& req, ReferenceSolution& out) const override;
};

// Reference provider variant for exact/manufactured solution studies:
// it builds the base FEM mesh with Dirichlet values sampled from u_exact on ALL boundary edges
// (outer boundary + hole boundaries). This avoids relying on manually-tagged boundary edges.
class FEMReferenceProviderExactDirichlet final : public IReferenceProvider {
    const PDEComponent* pde_ = nullptr;
    const PlanarMeshComponent* mesh_ = nullptr;
    std::function<double(double, double)> u_exact_;

public:
    FEMReferenceProviderExactDirichlet(
        const PDEComponent* pde,
        const PlanarMeshComponent* mesh,
        std::function<double(double, double)> u_exact
    ) : pde_(pde), mesh_(mesh), u_exact_(std::move(u_exact)) {}

    bool solve_reference(const ReferenceSolveRequest& req, ReferenceSolution& out) const override;
};

} // namespace fem
