#include <mainverte_core.h>

#include <iostream>
#include <string_view>

void mv::Platform::log(Level severity, std::string_view msg) {
    switch (severity) {
    case Level::Dbg: std::cout << "Dbg; " << msg; break;
    case Level::Inf: std::cout << "Inf; " << msg; break;
    case Level::Wrn: std::cout << "Wrn; " << msg; break;
    case Level::Err: std::cout << "Err; " << msg; break;
    }
}
