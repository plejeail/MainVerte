// include/mv_move_only_function.h --------------------------------------------
// MAINVERTE_CORE MOVE ONLY FUNCTIONS
//
// Owning callable wrapper for jobs and callbacks with non-copyable captures.
// Uses std::move_only_function when available, otherwise a compatibility fallback
// for standard libraries such as those supplied by Android NDK <= 29.
//
// Use unqualified Result(Args...) signatures for portability across both versions.
// Default construction and nullptr produce an empty wrapper; check operator bool
// before invocation if emptiness is possible. Invoking an empty wrapper is invalid.
// Moving transfers the target's ownership; invocation does not consume the target.
// Reference captures remain borrowed: their referents must outlive all uses.
// The wrapper itself provides no thread dispatch or synchronization.
//
//-------------------------------------------------------------- mainverte/core
#pragma once

#include <functional>


#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L

namespace mv {

/// Standard implementation of the owning, move-only callable wrapper.
template<typename Signature>
using MoveOnlyFunction = std::move_only_function<Signature>;

}  // namespace mv

#else

#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>


namespace mv {

/// Compatibility implementation; one allocation per nonempty target.
/// Moves transfer ownership without allocation. Only unqualified signatures work.
template<typename Signature>
class MoveOnlyFunction;

template<typename Result, typename... Args>
class MoveOnlyFunction<Result(Args...)> {
    struct Target {
        virtual ~Target() = default;
        virtual Result invoke(Args... args) = 0;
    };

    template<typename Function>
    struct StoredTarget final : Target {
        template<typename F> requires std::is_constructible_v<Function, F>
        explicit StoredTarget(F&& function) : function(std::forward<F>(function)) {}

        Result invoke(Args... args) override {
            if constexpr (std::is_void_v<Result>) {
                std::invoke(function, std::forward<Args>(args)...);
            } else {
                return std::invoke(function, std::forward<Args>(args)...);
            }
        }

        Function function;
    };

public:
    MoveOnlyFunction() = default;
    MoveOnlyFunction(std::nullptr_t) noexcept {}  // NOLINT(misc-explicit-constructor)

    template<typename Function>
        requires (!std::is_same_v<std::remove_cvref_t<Function>, MoveOnlyFunction>
                  && std::is_constructible_v<std::decay_t<Function>, Function>
                  && std::is_invocable_r_v<Result, std::decay_t<Function>&, Args...>)
    MoveOnlyFunction(Function&& function) {  // NOLINT(misc-explicit-constructor)
        using F = std::decay_t<Function>;
        if constexpr (std::is_pointer_v<F> || std::is_member_pointer_v<F>) {
            if (function == nullptr) {
                return;
            }
        }

        target = std::make_unique<StoredTarget<F>>(std::forward<Function>(function));
    }

    MoveOnlyFunction(const MoveOnlyFunction&)                = delete;
    MoveOnlyFunction& operator=(const MoveOnlyFunction&)     = delete;
    MoveOnlyFunction(MoveOnlyFunction&&) noexcept            = default;
    MoveOnlyFunction& operator=(MoveOnlyFunction&&) noexcept = default;

    MoveOnlyFunction& operator=(std::nullptr_t) noexcept {
        target.reset();
        return *this;
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(target);
    }

    Result operator()(Args... args) {
        assert(target);
        return target->invoke(std::forward<Args>(args)...);
    }

private:
    std::unique_ptr<Target> target;
};

}  // namespace mv

#endif  // support move_only_function
