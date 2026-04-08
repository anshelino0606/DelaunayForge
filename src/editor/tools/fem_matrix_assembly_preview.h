#ifndef FEM_MATRIX_ASSEMBLY_PREVIEW_H
#define FEM_MATRIX_ASSEMBLY_PREVIEW_H

#include <cstdint>
#include <concepts>
#include <limits>
#include <vector>
#include <span>
#include <algorithm>

namespace fem {

template <std::floating_point Real>
struct SparseMatrixCOO {
    using index_type = std::int32_t;

    std::vector<index_type> row;
    std::vector<index_type> col;
    std::vector<Real>       val;

    index_type rows = 0;
    index_type cols = 0;

    [[nodiscard]] std::size_t nnz() const noexcept { return val.size(); }

    void reserve(std::size_t n) {
        row.reserve(n);
        col.reserve(n);
        val.reserve(n);
    }

    void add_entry(index_type i, index_type j, Real a) {
        row.push_back(i);
        col.push_back(j);
        val.push_back(a);
    }

    [[nodiscard]] bool consistent() const noexcept {
        return row.size() == col.size() && col.size() == val.size();
    }
};

struct AssemblyStatistics {
    std::size_t total_elements = 0;
    std::size_t total_dofs     = 0;
    std::size_t matrix_nnz     = 0;
    std::size_t bandwidth      = 0;
    double      assembly_time_ms = 0.0; 

    bool        symmetric_hint = true;

    [[nodiscard]] double sparsity() const noexcept {
        if (total_dofs == 0) return 0.0;
        const double n = static_cast<double>(total_dofs);
        return 1.0 - static_cast<double>(matrix_nnz) / (n * n);
    }
};


struct MatrixPreviewData {
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> occ;

    bool has_values = false;
    std::vector<float> values;
    float min_value = 0.0f;
    float max_value = 0.0f;

    AssemblyStatistics stats{};

    [[nodiscard]] bool valid() const noexcept {
        return w > 0 && h > 0 && static_cast<std::size_t>(w * h) == occ.size();
    }
};

struct PreviewConfig {
    int  max_size    = 256;
    bool include_values = false;
};


struct FEMProblem;


template <std::floating_point Real>
struct AssemblyResult {
    SparseMatrixCOO<Real> K;
    std::vector<Real>     rhs;
    AssemblyStatistics    stats;
};


template <std::floating_point Real = double>
class FEMMatrixAssembler final {
public:
    using Matrix = SparseMatrixCOO<Real>;
    using Vector = std::vector<Real>;
    using Index  = typename Matrix::index_type;

    [[nodiscard]] static AssemblyResult<Real> assemble(const FEMProblem& problem);

    [[nodiscard]] static MatrixPreviewData make_preview(const Matrix& K,
                                                        const AssemblyStatistics& stats,
                                                        PreviewConfig cfg = {});

private:
    static void assemble_element(const FEMProblem& problem,
                                 std::size_t elem_idx,
                                 Matrix& K,
                                 Vector& rhs);

    static void apply_boundary_conditions(const FEMProblem& problem,
                                          Matrix& K,
                                          Vector& rhs);

    static void compute_basic_statistics(const Matrix& K, AssemblyStatistics& stats) noexcept;

    [[nodiscard]] static MatrixPreviewData downsample_occupancy(const Matrix& K,
                                                                const AssemblyStatistics& stats,
                                                                PreviewConfig cfg);
};

} // namespace fem

#endif // FEM_MATRIX_ASSEMBLY_PREVIEW_H
