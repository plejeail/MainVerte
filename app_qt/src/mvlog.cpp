#include "mvlog.h"

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

#include "platform.h"


#if !MV_LOG_DISABLED

[[nodiscard]] bool mvlog::detail::utc_time(std::tm& date, std::time_t timestamp) {
#ifdef _WIN32
    return ::gmtime_s(&date, &timestamp) == 0;
#else
    return ::gmtime_r(&timestamp, &date) != nullptr;
#endif
}

[[nodiscard]] bool mvlog::detail::system_local_time(std::tm& date, std::time_t timestamp) {
#ifdef _WIN32
    return ::localtime_s(&date, &timestamp) == 0;
#else
    return ::localtime_r(&timestamp, &date) != nullptr;
#endif
}

[[nodiscard]] bool mvlog::detail::LocalOffsetCache::get(std::time_t timestamp, std::time_t& offset) {
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

// Interpret calendar fields as UTC, without consulting the system time zone.
std::chrono::sys_seconds mvlog::detail::calendar_seconds(const std::tm& date) {
    using namespace std::chrono;
    const year_month_day day{year{date.tm_year + 1900},
                             month{static_cast<unsigned>(date.tm_mon + 1)},
                             std::chrono::day{static_cast<unsigned>(date.tm_mday)}};
    return sys_days{day} + hours{date.tm_hour} + minutes{date.tm_min} + seconds{date.tm_sec};
}

[[nodiscard]] bool mvlog::detail::local_time(std::tm& date, std::time_t timestamp) {
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

[[nodiscard]] bool mvlog::detail::local_time(std::tm& date) {
    const auto now = std::time(nullptr);
    return now != static_cast<std::time_t>(-1) && local_time(date, now);
}

std::size_t mvlog::detail::estimate_record_size(std::size_t dynamic_size) noexcept {
    return 45 + dynamic_size;
}

// Write exactly two digits; the caller supplies two writable bytes.
void mvlog::detail::copy_time(char* destination, unsigned value) noexcept {
    assert(value <= 99);
    destination[0] = static_cast<char>('0' + value / 10);
    destination[1] = static_cast<char>('0' + value % 10);
}

std::string mvlog::detail::format_record(char level, const std::tm& date, std::string_view message) {
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

[[nodiscard]] bool mvlog::detail::FileStream::open(const std::filesystem::path& filename) {
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

[[nodiscard]] bool mvlog::detail::FileStream::flush() {
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

[[nodiscard]] bool mvlog::detail::FileStream::close() noexcept {
    if (_fd == nullptr) [[unlikely]] {
        return true;
    }

    int res = std::fclose(_fd);
    _fd = nullptr;
    return res == 0;
}

[[nodiscard]] bool mvlog::detail::FileStream::write(const char* buf, std::size_t bufsize) {
    assert(_fd != nullptr);
    assert(buf != nullptr || bufsize == 0);
    return std::fwrite(buf, 1, bufsize, _fd) == bufsize;
}

[[nodiscard]] bool mvlog::detail::FileStream::find_max_file_index(const std::filesystem::path& filename,
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

[[nodiscard]] bool mvlog::detail::FileStream::rotate(std::size_t max_archives) {
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

[[nodiscard]] bool mvlog::detail::FileStream::compact_archives(std::size_t keep) {
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

namespace {

struct LogState {
    std::mutex mutex;
    mvlog::detail::FileStream file;
    std::uintmax_t size{0};
    std::uintmax_t max_size{0};
    std::size_t max_archives{0};
    bool initialized{false};
    bool file_enabled{false};
};

LogState& log_state() {
    static LogState log;
    return log;
}

void report_file_error(LogState& log) {
    log.file_enabled = false;
    (void)log.file.close();
    std::tm date{};
    if (!mvlog::detail::local_time(date)) {
        date.tm_mon = -1;
    }

    platform_output(
        mvlog::Level::Error,
        mvlog::detail::format_record('E', date, "mvlog: file output disabled after an I/O error.")
    );
}

}  // namespace

mvlog::InitResult mvlog::init(const char* logfile_path, std::size_t max_size_kB, short max_files) {
    auto& log = log_state();
    const std::lock_guard lock(log.mutex);
    if (log.initialized) {
        return InitResult::AlreadyInitialized;
    }

    if (logfile_path == nullptr || *logfile_path == '\0' || max_files < 0 ||
        max_size_kB > std::numeric_limits<std::uintmax_t>::max() / 1024) {
        return InitResult::InvalidArgument;
    }

    const std::string_view utf8{logfile_path};
    const std::filesystem::path path{std::u8string{
        reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()}};
    if (!log.file.open(path)) {
        return InitResult::FileError;
    }

    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        (void)log.file.close();
        return InitResult::FileError;
    }

    log.size = size;
    log.max_size = static_cast<std::uintmax_t>(max_size_kB) * 1024;
    log.max_archives = static_cast<std::size_t>(max_files);
    log.file_enabled = true;
    log.initialized = true;
    return InitResult::Ok;
}

void mvlog::detail::write(const mvlog::Level level, const std::string_view message) {
    if (!enabled(level)) {
        return;
    }

    auto& log = log_state();
    const std::lock_guard lock(log.mutex);
    std::tm date{};
    if (!local_time(date)) {
        // Keep the message even when the clock cannot be converted.
        date.tm_mon = -1;
    }

    const auto record = format_record(static_cast<char>(level), date, message);
#if MV_LOG_ENABLE_CONSOLE
    platform_output(level, record);
#endif
    if (!log.file_enabled) {
        return;
    }

    // Keep a record intact even if it alone exceeds the configured limit.
    if (log.max_size != 0 && log.size != 0 &&
        (log.size >= log.max_size || record.size() > log.max_size - log.size)) {
        if (!log.file.rotate(log.max_archives)) {
            report_file_error(log);
            return;
        }

        log.size = 0;
    }

    if (!log.file.write(record.data(), record.size()) || !log.file.flush()) {
        report_file_error(log);
        return;
    }

    log.size += record.size();
}

#endif
