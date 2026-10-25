// src/tools.h ----------------------------------------------------------------
// MISC & TOOLS
//
// Small things that are domain independant and may be used anywhere.
// ------------------------------------------------------------- mainverte/core
#pragma once

#include "mainverte_core.h"


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
    mv::Platform::log(mv::Level::Dbg, message);
}

MV_INLINE void log_inf(std::string_view message) {
    assert(message.size() > 0ULL && message.size() < 1000ULL);
    mv::Platform::log(mv::Level::Inf, message);
}

MV_INLINE void log_wrn(std::string_view message) {
    assert(message.size() > 0ULL && message.size() < 1000ULL);
    mv::Platform::log(mv::Level::Wrn, message);
}

MV_INLINE void log_err(std::string_view message) {
    assert(message.size() > 0ULL && message.size() < 1000ULL);
    mv::Platform::log(mv::Level::Err, message);
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

class Stopwatch {
    using clock = std::chrono::steady_clock;

public:
    explicit Stopwatch(const char* title)
    : title(title)
    , start(clock::now())
    {}

    ~Stopwatch() {
        const auto elapsed = std::chrono::duration<double, std::milli>(clock::now() - start).count();
        assert(elapsed >= 0);
        log_dbg("{} {}ms\n", title, elapsed);
    }

    Stopwatch(const Stopwatch&)            = delete;
    Stopwatch& operator=(const Stopwatch&) = delete;
    Stopwatch(Stopwatch&&)                 = delete;
    Stopwatch& operator=(Stopwatch&&)      = delete;

private:
    const char*       title;
    clock::time_point start;
};

#define MEASURE(title) Stopwatch stopwatch##__COUNTER__(title)
