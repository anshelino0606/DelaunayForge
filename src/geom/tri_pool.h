#pragma once

#include "common_types_2d.h"

namespace fem {

struct TriPool final {
    using id_type = int32_t;
    static constexpr id_type kInvalid = -1;

    using value_type = Tri;
    using iterator = std::vector<Tri>::iterator;
    using const_iterator = std::vector<Tri>::const_iterator;

    iterator begin() noexcept { return tris_.begin(); }
    iterator end()   noexcept { return tris_.end(); }
    const_iterator begin() const noexcept { return tris_.begin(); }
    const_iterator end()   const noexcept { return tris_.end(); }

    [[nodiscard]] std::size_t size() const noexcept { return tris_.size(); }
    [[nodiscard]] bool empty() const noexcept { return tris_.empty(); }

    TriPool& operator=(const std::vector<Tri>& vec) {
        tris_ = vec;
        free_.clear();
        return *this;
    }
    TriPool& operator=(std::vector<Tri>&& vec) noexcept {
        tris_ = std::move(vec);
        free_.clear();
        return *this;
    }

    std::vector<Tri> as_vector() const { return tris_; }


    TriPool() = default;

    void clear() noexcept {
        tris_.clear();
        free_.clear();
    }

    void reserve(std::size_t n_tris) {
        tris_.reserve(n_tris);
        free_.reserve(n_tris / 2);
    }

    [[nodiscard]] Tri& operator[](id_type id) noexcept {
        return tris_[static_cast<std::size_t>(id)];
    }
    [[nodiscard]] const Tri& operator[](id_type id) const noexcept {
        return tris_[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] bool alive(id_type id) const noexcept {
        if (id < 0) return false;
        const auto u = static_cast<std::size_t>(id);
        return u < tris_.size() && tris_[u].valid;
    }

    /// Allocate a triangle slot (reuses erased ids first).
    [[nodiscard]] id_type alloc(int a, int b, int c) {
        if (!free_.empty()) {
            const id_type id = free_.back();
            free_.pop_back();
            Tri& t = tris_[static_cast<std::size_t>(id)];
            t = Tri(a, b, c, id);
            t.valid = true;
            return id;
        }
        const id_type id = static_cast<id_type>(tris_.size());
        tris_.emplace_back(a, b, c, id);
        tris_.back().valid = true;
        return id;
    }

    /// Mark a triangle dead and make its id reusable.
    void erase(id_type id) noexcept {
        if (id < 0) return;
        const auto u = static_cast<std::size_t>(id);
        if (u >= tris_.size()) return;

        Tri& t = tris_[u];
        if (!t.valid) return;

        t.valid = false;
        free_.push_back(id);
    }

    /// Make storage dense again and re-assign ids [0..m-1].
    template <class KeepPred>
    void compact_keep_if(KeepPred&& keep) {
        std::vector<Tri> dense;
        dense.reserve(tris_.size());

        for (const Tri& src : tris_) {
            if (!src.valid) continue;
            if (!std::invoke(keep, src)) continue;

            Tri dst = src;
            dst.id = static_cast<int>(dense.size());
            dst.valid = true;
            dst.neighbors[0] = dst.neighbors[1] = dst.neighbors[2] = -1;
            dense.push_back(dst);
        }

        tris_.swap(dense);
        free_.clear();
    }

    /// keep all currently-valid triangles, just densify.
    void compact_valid_only() {
        compact_keep_if([](const Tri&) { return true; });
    }

private:
    std::vector<Tri>     tris_;
    std::vector<id_type> free_;
};

}