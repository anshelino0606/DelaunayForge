#pragma once

#include <variant>
#include <optional>

namespace fem {

template <typename... Ts> struct Visitor : Ts... { using Ts::operator()...; };
template <typename... Ts> Visitor(Ts...) -> Visitor<Ts...>;

template <typename... Types>
class Variant {
public:
    template <typename T>
    Variant(T&& value) : storage_(std::forward<T>(value)) {}

    [[nodiscard]] std::size_t index() const noexcept { return storage_.index(); }

    template <typename T>
    [[nodiscard]] bool is() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    template <typename T>
    [[nodiscard]] T* get() noexcept {
        return std::get_if<T>(&storage_);
    }

    template <typename T>
    [[nodiscard]] const T* get() const noexcept {
        return std::get_if<T>(&storage_);
    }

    template <typename T>
    [[nodiscard]] std::optional<T> try_get() const noexcept {
        if (const T* ptr = get<T>()) return *ptr;
        return std::nullopt;
    }

    template <typename... Visitors>
    auto visit(Visitors&&... visitors) & {
        return std::visit(Visitor{std::forward<Visitors>(visitors)...}, storage_);
    }

    template <typename... Visitors>
    auto visit(Visitors&&... visitors) const & {
        return std::visit(Visitor{std::forward<Visitors>(visitors)...}, storage_);
    }

    template<typename CustomVisitor>
    auto apply(CustomVisitor&& visitor) & {
        return std::visit(visitor, storage_);
    }

    template<typename CustomVisitor>
    auto apply(CustomVisitor&& visitor) const & {
        return std::visit(visitor, storage_);
    }

private:
    std::variant<Types...> storage_;
};

}