#include "../src/mvlog/mvlog.inl.h"

#include <doctest/doctest.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace {
class TemporaryDirectory {
public:
    TemporaryDirectory() {
        std::error_code error;
        const auto root = std::filesystem::temp_directory_path(error);
        REQUIRE_FALSE(error);
        const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto candidate = root / ("mainverte-log-test-" + std::to_string(seed) +
                                           "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate, error)) {
                path = candidate;
                return;
            }
            REQUIRE_FALSE(error);
        }
        FAIL("Cannot create a unique temporary directory");
    }

    ~TemporaryDirectory() {
        // Only remove the unique directory successfully created by this test.
        if (!path.empty()) {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    std::filesystem::path path;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    const std::string contents{std::istreambuf_iterator<char>{input},
                               std::istreambuf_iterator<char>{}};
    REQUIRE_FALSE(input.bad());
    return contents;
}
}

TEST_CASE("FileStream flush and close are harmless when closed") {
    mvlog::detail::FileStream stream;
    CHECK(stream.flush());
    CHECK(stream.close());
    CHECK(stream.close());
}

TEST_CASE("FileStream writes exact bytes and flush makes them readable before close") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "journal.log";
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    const std::string message{"line\r\nwith\0binary\n", 18};
    REQUIRE(stream.write(message.data(), message.size()));
    REQUIRE(stream.flush());
    CHECK(read_file(path) == message);
    REQUIRE(stream.close());
    CHECK(stream.close());
    CHECK(stream.flush());
    CHECK(read_file(path) == message);
}

TEST_CASE("FileStream close and destruction flush buffered data and reopening appends") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "journal.log";
    {
        mvlog::detail::FileStream stream;
        REQUIRE(stream.open(path));
        REQUIRE(stream.write("first", 5));
        REQUIRE(stream.close());
        CHECK(read_file(path) == "first");
        REQUIRE(stream.open(path));
        REQUIRE(stream.write(" second", 7));
    }
    CHECK(read_file(path) == "first second");
    {
        mvlog::detail::FileStream stream;
        REQUIRE(stream.open(path));
        REQUIRE(stream.write(" third", 6));
    }
    CHECK(read_file(path) == "first second third");
}

TEST_CASE("FileStream refuses a second open without losing the original file") {
    TemporaryDirectory temporary;
    const auto first = temporary.path / "first.log";
    const auto second = temporary.path / "second.log";
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(first));
    CHECK_FALSE(stream.open(second));
    REQUIRE(stream.write("original", 8));
    REQUIRE(stream.close());
    CHECK(read_file(first) == "original");
    std::error_code error;
    CHECK_FALSE(std::filesystem::exists(second, error));
    CHECK_FALSE(error);
}

TEST_CASE("FileStream can recover after opening an invalid path") {
    TemporaryDirectory temporary;
    mvlog::detail::FileStream stream;
    CHECK_FALSE(stream.open(temporary.path / "missing-directory" / "journal.log"));
    CHECK(stream.flush());
    CHECK(stream.close());
    CHECK_FALSE(stream.open(temporary.path));
    const auto path = temporary.path / "valid.log";
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("ok", 2));
    REQUIRE(stream.close());
    CHECK(read_file(path) == "ok");
}

TEST_CASE("FileStream supports Unicode directory and file names") {
    TemporaryDirectory temporary;
    const auto directory = temporary.path / std::filesystem::path{u8"jardin-\u00e9t\u00e9-\u82b1"};
    std::error_code error;
    REQUIRE(std::filesystem::create_directory(directory, error));
    REQUIRE_FALSE(error);
    const auto path = directory / std::filesystem::path{u8"r\u00e9coltes-\U0001f331.log"};
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("unicode path", 12));
    REQUIRE(stream.close());
    CHECK(read_file(path) == "unicode path");
}

TEST_CASE("FileStream accepts an empty write to an open file") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "empty.log";
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("", 0));
    REQUIRE(stream.close());
    CHECK(read_file(path).empty());
}

namespace {
void create_log(const std::filesystem::path& path, const std::string& contents) {
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    REQUIRE(stream.write(contents.data(), contents.size()));
    REQUIRE(stream.close());
}
}

TEST_CASE("Rotation appends increasing ids after the complete filename") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "log.txt";
    mvlog::detail::FileStream stream;
    CHECK_FALSE(stream.rotate());
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("first", 5));
    REQUIRE(stream.rotate());
    CHECK(read_file(temporary.path / "log.txt.1") == "first");
    CHECK(read_file(path).empty());
    REQUIRE(stream.write("second", 6));
    REQUIRE(stream.rotate());
    REQUIRE(stream.close());
    CHECK(read_file(temporary.path / "log.txt.1") == "first");
    CHECK(read_file(temporary.path / "log.txt.2") == "second");
    CHECK(read_file(path).empty());
}

TEST_CASE("Initialization finds the largest numeric suffix and ignores unrelated names") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "log.txt";
    create_log(temporary.path / "log.txt.2", "old");
    create_log(temporary.path / "log.txt.10", "newer");
    create_log(temporary.path / "log.txt.100.tmp", "unrelated");
    create_log(temporary.path / "other.txt.200", "unrelated");
    std::uint64_t index{99};
    REQUIRE(mvlog::detail::FileStream::find_max_file_index(path, index));
    CHECK(index == 10);
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("current", 7));
    REQUIRE(stream.rotate());
    REQUIRE(stream.close());
    CHECK(read_file(temporary.path / "log.txt.11") == "current");
    CHECK(read_file(temporary.path / "log.txt.10") == "newer");
    REQUIRE(stream.open(path));
    REQUIRE(stream.rotate());
    REQUIRE(stream.close());
    CHECK(read_file(temporary.path / "log.txt.12").empty());
}

TEST_CASE("Rotation refuses a destination created after initialization without replacing it") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "log.txt";
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    create_log(temporary.path / "log.txt.1", "preserve");
    CHECK_FALSE(stream.rotate());
    REQUIRE(stream.write("still open", 10));
    REQUIRE(stream.close());
    CHECK(read_file(path) == "still open");
    CHECK(read_file(temporary.path / "log.txt.1") == "preserve");
}

TEST_CASE("Archive scan handles empty directories, missing directories and index exhaustion") {
    TemporaryDirectory temporary;
    std::uint64_t index{99};
    REQUIRE(mvlog::detail::FileStream::find_max_file_index(temporary.path / "log", index));
    CHECK(index == 0);
    index = 99;
    CHECK_FALSE(mvlog::detail::FileStream::find_max_file_index(temporary.path / "missing" / "log", index));
    CHECK(index == 99);
    create_log(temporary.path / "log.18446744073709551615", "last archive");
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(temporary.path / "log"));
    CHECK_FALSE(stream.rotate());
    REQUIRE(stream.write("preserved", 9));
    REQUIRE(stream.close());
    CHECK(read_file(temporary.path / "log") == "preserved");
    create_log(temporary.path / "log.18446744073709551616", "overflow");
    CHECK_FALSE(mvlog::detail::FileStream::find_max_file_index(temporary.path / "log", index));
    CHECK(index == 99);
}

TEST_CASE("Rotation preserves Unicode paths and supports filenames without extensions") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / std::filesystem::path{u8"\u82b1-\u00e9t\u00e9"};
    auto archive = path;
    archive += ".1";
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("unicode", 7));
    REQUIRE(stream.rotate());
    REQUIRE(stream.close());
    CHECK(read_file(archive) == "unicode");
    CHECK(read_file(path).empty());
}

TEST_CASE("Bounded rotation discards oldest and shifts archives towards index one") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "log.txt";
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    for (const char message : {'a', 'b', 'c', 'd', 'e'}) {
        REQUIRE(stream.write(&message, 1));
        REQUIRE(stream.rotate(3));
    }
    REQUIRE(stream.close());
    CHECK(read_file(temporary.path / "log.txt.1") == "c");
    CHECK(read_file(temporary.path / "log.txt.2") == "d");
    CHECK(read_file(temporary.path / "log.txt.3") == "e");
    CHECK_FALSE(std::filesystem::exists(temporary.path / "log.txt.4"));
    CHECK(read_file(path).empty());
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("f", 1));
    REQUIRE(stream.rotate(3));
    REQUIRE(stream.close());
    CHECK(read_file(temporary.path / "log.txt.1") == "d");
    CHECK(read_file(temporary.path / "log.txt.3") == "f");
}

TEST_CASE("A single archive limit replaces the old archive with current") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "log";
    create_log(temporary.path / "log.1", "old");
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("new", 3));
    REQUIRE(stream.rotate(1));
    REQUIRE(stream.close());
    CHECK(read_file(temporary.path / "log.1") == "new");
    CHECK_FALSE(std::filesystem::exists(temporary.path / "log.2"));
}

TEST_CASE("Bounded rotation handles gaps and a reduced limit without touching unrelated files") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "log";
    create_log(temporary.path / "log.2", "old");
    create_log(temporary.path / "log.5", "middle");
    create_log(temporary.path / "log.9", "recent");
    create_log(temporary.path / "log.100.tmp", "unrelated");
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("current", 7));
    REQUIRE(stream.rotate(2));
    REQUIRE(stream.close());
    CHECK(read_file(temporary.path / "log.1") == "recent");
    CHECK(read_file(temporary.path / "log.2") == "current");
    CHECK_FALSE(std::filesystem::exists(temporary.path / "log.5"));
    CHECK_FALSE(std::filesystem::exists(temporary.path / "log.9"));
    CHECK(read_file(temporary.path / "log.100.tmp") == "unrelated");
}

TEST_CASE("Bounded rotation refuses non-file archives before deleting old logs") {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "log";
    create_log(temporary.path / "log.1", "preserve");
    std::error_code error;
    REQUIRE(std::filesystem::create_directory(temporary.path / "log.2", error));
    REQUIRE_FALSE(error);
    mvlog::detail::FileStream stream;
    REQUIRE(stream.open(path));
    REQUIRE(stream.write("current", 7));
    CHECK_FALSE(stream.rotate(2));
    CHECK(read_file(temporary.path / "log.1") == "preserve");
    CHECK(read_file(path) == "current");
    CHECK(std::filesystem::is_directory(temporary.path / "log.2"));
}
