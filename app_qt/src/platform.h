#pragma once

#include <string>


namespace mvlog { enum class Level : char; }

void wait_for_debugger(int argc, char** argv);

void platform_output(mvlog::Level level, const std::string& record);
