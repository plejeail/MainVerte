// clazy:skip
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <doctest/doctest.h>

#include "../src/mvlog/mvlog.h"


namespace {

std::string read_log(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

class CaptureConsole {
public:
    explicit CaptureConsole(const std::filesystem::path& path) {
        (void)std::fflush(stderr);
#ifdef _WIN32
        saved = ::_dup(::_fileno(stderr));
        REQUIRE(saved >= 0);
        file = ::_wfopen(path.c_str(), L"wb");
        REQUIRE(file != nullptr);
        REQUIRE(::_dup2(::_fileno(file), ::_fileno(stderr)) == 0);
#else
        saved = ::dup(::fileno(stderr));
        REQUIRE(saved >= 0);
        file = std::fopen(path.c_str(), "wb");
        REQUIRE(file != nullptr);
        REQUIRE(::dup2(::fileno(file), ::fileno(stderr)) >= 0);
#endif
    }
    ~CaptureConsole() {
        (void)std::fflush(stderr);
#ifdef _WIN32
        (void)::_dup2(saved, ::_fileno(stderr));
        (void)::_close(saved);
#else
        (void)::dup2(saved, ::fileno(stderr));
        (void)::close(saved);
#endif
        (void)std::fclose(file);
    }
private:
    int saved{-1};
    std::FILE* file{nullptr};
};

}  // namespace

TEST_CASE("Desktop logger initializes, formats, rotates, and falls back to console") {
    std::error_code error;
    const auto root = std::filesystem::temp_directory_path(error);
    REQUIRE_FALSE(error);
    const auto directory = root / ("mvlog-desktop-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(std::filesystem::create_directory(directory, error));
    REQUIRE_FALSE(error);
    // The file sink remains open until process exit, so cleanup is performed
    // by the test's explicit failure path below, which closes the sink.
    const auto path = directory / std::filesystem::path{u8"\u82b1.log"};
    const auto console = directory / "console.log";
    {
        CaptureConsole capture(console);
        mvlog::inf("before init");
        CHECK(mvlog::init(nullptr, 1, 0) == mvlog::InitResult::InvalidArgument);
        CHECK(mvlog::init("", 1, 0) == mvlog::InitResult::InvalidArgument);
        CHECK(mvlog::init("unused.log", 1, -1) == mvlog::InitResult::InvalidArgument);
        const auto missing = (directory / "missing" / "log").u8string();
        CHECK(mvlog::init(reinterpret_cast<const char*>(missing.c_str()), 1, 0) == mvlog::InitResult::FileError);
        const auto utf8 = path.u8string();
        REQUIRE(mvlog::init(reinterpret_cast<const char*>(utf8.c_str()), 1, 2) == mvlog::InitResult::Ok);
        CHECK(mvlog::init(reinterpret_cast<const char*>(utf8.c_str()), 1, 0) == mvlog::InitResult::AlreadyInitialized);
        mvlog::dbg("answer {}", 42);
        const auto record = read_log(path);
        REQUIRE(record.size() == 26);
        CHECK(record[0] == 'D');
        CHECK(record[1] == ' ');
        CHECK(record[6] == ' ');
        CHECK(record[9] == ':');
        CHECK(record[12] == ':');
        CHECK(record.substr(15) == " answer 42\n");
        mvlog::wrn(std::string(1100, 'x'));
        auto first = path;
        first += ".1";
        CHECK(read_log(first) == record);
        CHECK(read_log(path).size() == 1117);
        mvlog::err("next");
        auto second = path;
        second += ".2";
        CHECK(read_log(second).size() == 1117);
        CHECK(read_log(path).substr(16) == "next\n");
        // At the limit, discard .1, move .2 to .1 and archive current as .2.
        mvlog::inf(std::string(1100, 'y'));
        CHECK(read_log(first).size() == 1117);
        CHECK(read_log(second).substr(16) == "next\n");
        CHECK(read_log(path).size() == 1117);
        // An unexpected directory must stop rotation without deleting it.
        auto blocked = path;
        blocked += ".3";
        REQUIRE(std::filesystem::create_directory(blocked, error));
        REQUIRE_FALSE(error);
        mvlog::inf("rotation fails");
        CHECK(std::filesystem::is_directory(blocked));
        mvlog::err("console survives");
    }
    auto output = read_log(console);
    // Windows console streams translate LF into CRLF.
    std::erase(output, '\r');
    CHECK(output.find("before init\n") != std::string::npos);
    CHECK(output.find("answer 42\n") != std::string::npos);
    CHECK(output.find("file output disabled") != std::string::npos);
    CHECK(output.find("console survives\n") != std::string::npos);
    std::filesystem::remove_all(directory, error);
    CHECK_FALSE(error);
}
