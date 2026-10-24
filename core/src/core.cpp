#include "mainverte_core.h"

#include <memory>

using namespace core;


std::unique_ptr<IPlatform> g_platform;

void core::initialize_platform(std::unique_ptr<IPlatform> implementation) {
    g_platform.swap(implementation);
}
