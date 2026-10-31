// src/tools.h ----------------------------------------------------------------
// MISC & TOOLS
//
// Small things that are domain independant and may be used anywhere.
// ------------------------------------------------------------- mainverte/core
#pragma once

#include <cassert>
#include <chrono>
#include <format>
#include <ratio>
#include <string_view>
#include <utility>

#include "mv_core.h"
#include "mv_move_only_function.h"


// Request forced inlining where supported; the compiler has the final say.
#if defined(_MSC_VER)
#  define MV_INLINE   __forceinline
#  define MV_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
#  define MV_INLINE   inline __attribute__((always_inline))
#  define MV_NOINLINE __attribute((noinline))
#else
#  define MV_INLINE inline
#  define MV_NOINLINE
#endif

MV_INLINE void log_dbg(std::string_view message) {
    assert(message.size() > 0ULL && message.size() < 1000ULL);
    mv::platform::log(mv::LogLevel::Dbg, message);
}

MV_INLINE void log_inf(std::string_view message) {
    assert(message.size() > 0ULL && message.size() < 1000ULL);
    mv::platform::log(mv::LogLevel::Inf, message);
}

MV_INLINE void log_wrn(std::string_view message) {
    assert(message.size() > 0ULL && message.size() < 1000ULL);
    mv::platform::log(mv::LogLevel::Wrn, message);
}

MV_INLINE void log_err(std::string_view message) {
    assert(message.size() > 0ULL && message.size() < 1000ULL);
    mv::platform::log(mv::LogLevel::Err, message);
}

template<typename... Args> requires (sizeof...(Args) > 0)
void log_dbg(std::format_string<Args...> format, Args&&... args) {
    log_dbg(std::format(format, std::forward<Args>(args)...));
}

template<typename... Args> requires (sizeof...(Args) > 0)
void log_inf(std::format_string<Args...> format, Args&&... args) {
    log_inf(std::format(format, std::forward<Args>(args)...));
}

template<typename... Args> requires (sizeof...(Args) > 0)
MV_NOINLINE void log_wrn(std::format_string<Args...> format, Args&&... args) {
    log_wrn(std::format(format, std::forward<Args>(args)...));
}

template<typename... Args> requires (sizeof...(Args) > 0)
MV_NOINLINE void log_err(std::format_string<Args...> format, Args&&... args) {
    log_err(std::format(format, std::forward<Args>(args)...));
}

template<typename T>
void dispatch_result(mv::MoveOnlyFunction<void(T)> callback, T result) {
    mv::platform::dispatch_callback(
        [callback = std::move(callback), result = std::move(result)]() mutable {
        callback(std::move(result));
    });
}

class Stopwatch {
    using clock = std::chrono::steady_clock;

public:
    explicit Stopwatch(const char* title)
    : _title(title)
    , _start(clock::now())
    {}

    ~Stopwatch() {
        if (_title != nullptr) {
            stop();
        }
    }

    void stop() {
        const auto elapsed = std::chrono::duration<double, std::milli>(clock::now() - _start).count();
        assert(elapsed >= 0);
        log_dbg("{} {}ms", _title, elapsed);
        _title = nullptr;
    }

    Stopwatch(const Stopwatch&)            = delete;
    Stopwatch& operator=(const Stopwatch&) = delete;
    Stopwatch(Stopwatch&&)                 = delete;
    Stopwatch& operator=(Stopwatch&&)      = delete;

private:
    const char*       _title;
    clock::time_point _start;
};

#define MEASURE(title) Stopwatch stopwatch##__COUNTER__(title)
