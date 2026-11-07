#if !(__ANDROID__)

#include "mvlog.h"

#include <cstdio>


void wait_for_debugger([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {}

void platform_output([[maybe_unused]]mvlog::Level level, const std::string& record) {
    (void)std::fwrite(record.data(), 1, record.size(), stderr);
    (void)std::fflush(stderr);
}

#endif
