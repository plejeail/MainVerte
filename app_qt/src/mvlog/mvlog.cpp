#include "mvlog.inl.h"

#include <cstdio>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>


#if !MV_LOG_DISABLED

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

    mvlog::detail::platform_output(
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
