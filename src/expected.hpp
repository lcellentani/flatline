#pragma once
#include <cassert>
#include <utility>
#include <variant>

// A minimal, dependency-free stand-in for std::expected<T, E> (C++23, not
// yet available under this project's C++20 baseline). This project does not
// use C++ exceptions for recoverable error handling (see coding_style.md) --
// Expected<T, E> is the return-value alternative. Construct via ok()/err()
// (both forward their arguments and construct T/E in place, so neither type
// needs to be movable or copyable); check has_value() (or `if (result)`)
// before reading value()/error(). Swap for std::expected transparently if
// the project moves to C++23.
//
// Named Expected, not Result: gm_gauntlet.hpp already has its own domain-
// specific `gauntlet::Result` (a std::variant<Effects, Failure> alias) --
// this type is unrelated and generic, so it gets std::expected's own name
// instead of colliding with that one under a `using namespace gm;`.
template <typename T, typename E>
class [[nodiscard]] Expected {
public:
    template <typename... Args>
    [[nodiscard]] static constexpr Expected ok(Args&&... args) {
        return Expected(std::in_place_index<0>, std::forward<Args>(args)...);
    }

    template <typename... Args>
    [[nodiscard]] static constexpr Expected err(Args&&... args) {
        return Expected(std::in_place_index<1>, std::forward<Args>(args)...);
    }

    [[nodiscard]] constexpr bool has_value() const { return storage_.index() == 0; }
    constexpr explicit operator bool() const { return has_value(); }

    // Precondition: has_value(). Reading the wrong side is a caller bug, not
    // a recoverable error, so it's asserted rather than thrown.
    [[nodiscard]] constexpr const T& value() const {
        assert(has_value());
        return std::get<0>(storage_);
    }
    [[nodiscard]] constexpr T& value() {
        assert(has_value());
        return std::get<0>(storage_);
    }

    // Precondition: !has_value().
    [[nodiscard]] constexpr const E& error() const {
        assert(!has_value());
        return std::get<1>(storage_);
    }

private:
    template <typename... Args>
    constexpr Expected(std::in_place_index_t<0>, Args&&... args)
        : storage_(std::in_place_index<0>, std::forward<Args>(args)...) {}
    template <typename... Args>
    constexpr Expected(std::in_place_index_t<1>, Args&&... args)
        : storage_(std::in_place_index<1>, std::forward<Args>(args)...) {}

    std::variant<T, E> storage_;
};
