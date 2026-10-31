#include "mvlog.inl.h"

#include <cstdio>


void mvlog::detail::platform_output(Level, const std::string& record) {
    (void)std::fwrite(record.data(), 1, record.size(), stderr);
    (void)std::fflush(stderr);
}
