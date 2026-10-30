#pragma once

#include <cassert>
#include <chrono>

#include <QDebug>


class Stopwatch {
    using clock = std::chrono::steady_clock;

public:
    explicit Stopwatch(const char* title)
    : _title(title)
    , _start(clock::now())
    {}

    ~Stopwatch() {
        const auto elapsed = std::chrono::duration<double, std::milli>(clock::now() - _start).count();
        assert(elapsed >= 0);
        qDebug() << _title << elapsed << "ms";
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
