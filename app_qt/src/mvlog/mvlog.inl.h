#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "mvlog.h"


namespace mvlog::detail {

// stderr on desktop, native Logcat on Android.
void platform_output(Level level, const std::string& record);

[[nodiscard]] inline bool utc_time(std::tm& date, std::time_t timestamp) {
#ifdef _WIN32
    return ::gmtime_s(&date, &timestamp) == 0;
#else
    return ::gmtime_r(&timestamp, &date) != nullptr;
#endif
}

[[nodiscard]] inline bool system_local_time(std::tm& date, std::time_t timestamp) {
#ifdef _WIN32
    return ::localtime_s(&date, &timestamp) == 0;
#else
    return ::localtime_r(&timestamp, &date) != nullptr;
#endif
}

// Interpret calendar fields as UTC, without consulting the system time zone.
inline std::chrono::sys_seconds calendar_seconds(const std::tm& date) {
    using namespace std::chrono;
    const year_month_day day{year{date.tm_year + 1900},
                             month{static_cast<unsigned>(date.tm_mon + 1)},
                             std::chrono::day{static_cast<unsigned>(date.tm_mday)}};
    return sys_days{day} + hours{date.tm_hour} + minutes{date.tm_min} + seconds{date.tm_sec};
}

class LocalOffsetCache {
public:
    [[nodiscard]] bool get(std::time_t timestamp, std::time_t& offset) {
        using namespace std::chrono;
        const auto utc_day = floor<days>(sys_seconds{seconds{timestamp}});
        const std::lock_guard lock(_mutex);
        if (!_valid || utc_day != _utc_day) {
            std::tm utc{};
            std::tm local{};
            if (!utc_time(utc, timestamp) || !system_local_time(local, timestamp)) {
                return false;
            }
            _offset = static_cast<std::time_t>((calendar_seconds(local) - calendar_seconds(utc)).count());
            _utc_day = utc_day;
            _valid = true;
        }

        offset = _offset;
        return true;
    }

private:
    std::mutex _mutex;
    std::chrono::sys_days _utc_day{};
    std::time_t _offset{};
    bool _valid{false};
};

// One offset per UTC day, shared by all callers. DST/time-zone changes within
// that day are deliberately deferred until the next UTC day. A clock jump to
// another day also refreshes the cache, including jumps backwards.
// TZ/environment changes must not run concurrently with time conversion.
[[nodiscard]] inline bool local_time(std::tm& date, std::time_t timestamp) {
    static LocalOffsetCache cache;
    std::time_t offset{};
    if (!cache.get(timestamp, offset)) {
        return false;
    }

    if ((offset > 0 && timestamp > std::numeric_limits<std::time_t>::max() - offset) ||
        (offset < 0 && timestamp < std::numeric_limits<std::time_t>::min() - offset)) {
        return false;
    }

    std::tm converted{};
    if (!utc_time(converted, timestamp + offset)) {
        return false;
    }

    // gmtime describes the shifted timestamp, not the actual DST status.
    converted.tm_isdst = -1;
    date = converted;
    return true;
}

[[nodiscard]] inline bool local_time(std::tm& date) {
    const auto now = std::time(nullptr);
    return now != static_cast<std::time_t>(-1) && local_time(date, now);
}

inline std::size_t estimate_record_size(std::size_t dynamic_size) noexcept {
    return 45 + dynamic_size;
}

// Write exactly two digits; the caller supplies two writable bytes.
inline void copy_time(char* destination, unsigned value) noexcept {
    assert(value <= 99);
    destination[0] = static_cast<char>('0' + value / 10);
    destination[1] = static_cast<char>('0' + value % 10);
}

inline std::string format_record(char level, const std::tm& date, std::string_view message) {
    static constexpr std::string_view static_fmt{"D 0000 00:00:00 "};
    std::string record;
    record.reserve(static_fmt.size() + message.size());
    record.append(static_fmt);
    record[0] = level;
    copy_time(record.data() + 2,  static_cast<unsigned>(date.tm_mon + 1));
    copy_time(record.data() + 4,  static_cast<unsigned>(date.tm_mday));
    copy_time(record.data() + 7,  static_cast<unsigned>(date.tm_hour));
    copy_time(record.data() + 10, static_cast<unsigned>(date.tm_min));
    copy_time(record.data() + 13, static_cast<unsigned>(date.tm_sec));
    record.append(message);
    record += '\n';
    return record;
}

// std::tm uses months [0, 11] and years since 1900. For a two-digit year,
// pass (date.tm_year + 1900) % 100 to copy_time().

class FileStream {
    std::FILE* _fd{ nullptr };
    std::filesystem::path _filename;
    std::uint64_t _max_file_index{0};

public:
    explicit FileStream() noexcept = default;
    FileStream(const FileStream&) = delete;
    FileStream(FileStream&&) = delete;
    FileStream& operator=(const FileStream&) = delete;
    FileStream& operator=(FileStream&&) = delete;

    ~FileStream() noexcept {
        (void)close();
    }

    [[nodiscard]] bool open(const std::filesystem::path& filename) {
        if (_fd != nullptr) {
            return false;
        }

        std::error_code error;
        const auto absolute_filename = std::filesystem::absolute(filename, error);
        std::uint64_t index{};
        if (error || !find_max_file_index(absolute_filename, index)) {
            return false;
        }

#ifdef _WIN32
        _fd = ::_wfopen(absolute_filename.c_str(), L"ab");
#else
        _fd = std::fopen(absolute_filename.c_str(), "ab");
#endif

        if (_fd == nullptr) {
            return false;
        }

        _filename = absolute_filename;
        _max_file_index = index;
        return true;
    }

    [[nodiscard]] bool flush() {
        if (_fd == nullptr) [[unlikely]] {
            return true;
        }

        int res = std::fflush(_fd);
        if (res != 0) [[unlikely]] {
            (void)close();
            return false;
        }

        return true;
    }

    [[nodiscard]] bool close() noexcept {
        if (_fd == nullptr) [[unlikely]] {
            return true;
        }

        int res = std::fclose(_fd);
        _fd = nullptr;
        return res == 0;
    }

    [[nodiscard]] bool write(const char* buf, std::size_t bufsize) {
        assert(_fd != nullptr);
        assert(buf != nullptr || bufsize == 0);
        return std::fwrite(buf, 1, bufsize, _fd) == bufsize;
    }

    // Numeric suffixes reserve an index even for directories and symlinks.
    // On failure, max_index is unchanged. No directory recursion is needed.
    [[nodiscard]] static bool find_max_file_index(const std::filesystem::path& filename,
                                                 std::uint64_t& max_index) {
        auto directory = filename.parent_path();
        if (directory.empty()) {
            directory = ".";
        }

        auto prefix = filename.filename().native();
        prefix += std::filesystem::path{"."}.native();
        std::error_code error;
        std::filesystem::directory_iterator entry(directory, error);
        const std::filesystem::directory_iterator end;
        std::uint64_t found{};
        while (!error && entry != end) {
            const auto name = entry->path().filename().native();
            if (name.starts_with(prefix) && name.size() > prefix.size()) {
                const auto suffix = name.substr(prefix.size());
                const bool numeric = std::ranges::all_of(suffix, [](auto digit) {
                    return digit >= '0' && digit <= '9';
                });
                if (numeric) {
                    std::uint64_t index{};
                    for (const auto digit : suffix) {
                        const auto value = static_cast<unsigned>(digit - '0');
                        if (index > (std::numeric_limits<std::uint64_t>::max() - value) / 10) {
                            return false;
                        }

                        index = index * 10 + value;
                    }

                    found = std::max(found, index);
                }
            }

            entry.increment(error);
        }

        if (error) {
            return false;
        }

        max_index = found;
        return true;
    }

    // Caller serializes writes/rotation and exclusively owns this log family.
    // max_archives excludes the current file; 0 keeps all archives.
    // At the limit, keep the newest archives and renumber them from 1.
    // After an I/O failure, some archive moves may already have succeeded.
    [[nodiscard]] bool rotate(std::size_t max_archives = 0) {
        if (_fd == nullptr ||
            (max_archives == 0 && _max_file_index == std::numeric_limits<std::uint64_t>::max())) {
            return false;
        }

        if (max_archives != 0 && _max_file_index >= max_archives) {
            if (!close() || !compact_archives(max_archives - 1)) {
                return false;
            }
        }

        auto archive = _filename;
        archive += "." + std::to_string(_max_file_index + 1);
        std::error_code error;
        const auto status = std::filesystem::symlink_status(archive, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            return false;
        }

        if (std::filesystem::exists(status)) {
            return false;
        }

        if (!close()) {
            return false;
        }

        error.clear();
        std::filesystem::rename(_filename, archive, error);
        if (error) {
            return false;
        }

        ++_max_file_index;
#ifdef _WIN32
        _fd = ::_wfopen(_filename.c_str(), L"ab");
#else
        _fd = std::fopen(_filename.c_str(), "ab");
#endif
        return _fd != nullptr;
    }

private:
    // Validate the full set before deleting or moving anything. Only canonical
    // numeric archives belonging to this log are touched; never follow links.
    [[nodiscard]] bool compact_archives(std::size_t keep) {
        struct Archive {
            std::uint64_t index;
            std::filesystem::path path;
        };

        std::vector<Archive> archives;
        auto prefix = _filename.filename().native();
        prefix += std::filesystem::path{"."}.native();
        std::error_code error;
        std::filesystem::directory_iterator entry(_filename.parent_path(), error);
        const std::filesystem::directory_iterator end;
        while (!error && entry != end) {
            const auto name = entry->path().filename().native();
            if (name.starts_with(prefix) && name.size() > prefix.size()) {
                const auto suffix = name.substr(prefix.size());
                if (suffix.front() != '0' && std::ranges::all_of(suffix, [](auto digit) {
                        return digit >= '0' && digit <= '9';
                    })) {
                    std::uint64_t index{};
                    for (const auto digit : suffix) {
                        const auto value = static_cast<unsigned>(digit - '0');
                        if (index > (std::numeric_limits<std::uint64_t>::max() - value) / 10) {
                            return false;
                        }
                        index = index * 10 + value;
                    }

                    const auto status = entry->symlink_status(error);
                    if (error || !std::filesystem::is_regular_file(status)) {
                        return false;
                    }

                    archives.push_back({.index=index, .path=entry->path()});
                }
            }

            entry.increment(error);
        }

        if (error) {
            return false;
        }

        std::ranges::sort(archives, {}, &Archive::index);
        const auto discard = archives.size() > keep ? archives.size() - keep : 0;
        for (std::size_t i = 0; i < discard; ++i) {
            if (!std::filesystem::remove(archives[i].path, error) || error) {
                return false;
            }
        }

        for (std::size_t i = discard; i < archives.size(); ++i) {
            auto destination = _filename;
            destination += "." + std::to_string(i - discard + 1);
            if (archives[i].path != destination) {
                std::filesystem::rename(archives[i].path, destination, error);
                if (error) {
                    return false;
                }
            }
        }

        _max_file_index = archives.size() - discard;
        return true;
    }
};

}  // namespace mvlog::detail
