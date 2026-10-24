//===-- include/core.h ----------------------------------------- mainverte/core
//
//
//===--------------------------------------------------------------------------
#pragma once

#include <memory>


namespace core {
class IPlatform {
public:
    virtual ~IPlatform() = default;
};

void initialize_platform(std::unique_ptr<IPlatform> implementation);
}
