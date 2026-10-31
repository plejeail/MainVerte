// Android NDK <= 29 does not support std::move_only_function
#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>


namespace mv {

// Owning callable for jobs and continuations, including non-copyable captures.
// One allocation per nonempty target; moving transfers ownership without allocation.
// Supports unqualified signatures only. Calling an empty function is invalid.
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

} // namespace mv
