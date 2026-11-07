#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <mutex>
#include <string_view>
#include <utility>


#ifndef MV_LOG_QT_TYPES_FORMAT
#define MV_LOG_QT_TYPES_FORMAT 0
#endif
#ifndef MV_LOG_QT_DISABLE_QDEBUG
#define MV_LOG_QT_DISABLE_QDEBUG 0
#endif
#ifndef MV_LOG_ENABLE_CONSOLE
#define MV_LOG_ENABLE_CONSOLE 1
#endif
#ifndef MV_LOG_ENABLE_DBG
#define MV_LOG_ENABLE_DBG 1
#endif
#ifndef MV_LOG_ENABLE_INF
#define MV_LOG_ENABLE_INF 1
#endif
#ifndef MV_LOG_ENABLE_WRN
#define MV_LOG_ENABLE_WRN 1
#endif
#ifndef MV_LOG_ENABLE_ERR
#define MV_LOG_ENABLE_ERR 1
#endif
#ifndef MV_LOG_DISABLED
#define MV_LOG_DISABLED 0
#endif

namespace mvlog {
enum class InitResult : int8_t { Ok, InvalidArgument, AlreadyInitialized, FileError };
// UTF-8 path supplied by the app (use its private data directory on Android); size is in KiB, 0 disables size-based rotation.
// max_files limits archives (excluding the current file); 0 retains them all.
// Initialization is allowed once; failure leaves console/Logcat output available.
[[nodiscard]] InitResult init(const char* logfile_path, std::size_t max_size_kB, short max_files);
enum class Level : char { Debug = 'D', Info = 'I', Warning = 'W', Error = 'E' };

namespace detail {
inline std::atomic<Level> runtime_level{Level::Debug};

constexpr int priority(Level level) {
    switch (level) {
    case Level::Debug: return 0;
    case Level::Info: return 1;
    case Level::Warning: return 2;
    case Level::Error: return 3;
    }

    return 3;
}

void write(const Level lvl, const std::string_view message);
}  //  namespace detail

// Thread-safe threshold for all sinks. Debug is used until saved settings load.
// In-flight writes may finish while another thread changes the threshold.
inline void set_level(Level level) {
    detail::runtime_level.store(level, std::memory_order_relaxed);
}

[[nodiscard]] inline Level level() {
    return detail::runtime_level.load(std::memory_order_relaxed);
}

[[nodiscard]] inline bool enabled(Level severity) {
#if MV_LOG_DISABLED
    return false;
#endif  // MV_LOG_DISABLED

    switch (severity) {
    case Level::Debug:   if (!MV_LOG_ENABLE_DBG) return false; break;  // NOLINT(bugprone-branch-clone)
    case Level::Info:    if (!MV_LOG_ENABLE_INF) return false; break;  // NOLINT(bugprone-branch-clone)
    case Level::Warning: if (!MV_LOG_ENABLE_WRN) return false; break;  // NOLINT(bugprone-branch-clone)
    case Level::Error:   if (!MV_LOG_ENABLE_ERR) return false; break;  // NOLINT(bugprone-branch-clone)
    }

    return detail::priority(severity) >= detail::priority(level());
}

inline void dbg([[maybe_unused]] const std::string_view sw) {
#if MV_LOG_ENABLE_DBG && !MV_LOG_DISABLED
    if (enabled(Level::Debug)) {
        detail::write(Level::Debug, sw);
    }
#endif  // MV_LOG_ENABLE_DBG && !MV_LOG_DISABLED
}

inline void inf([[maybe_unused]] const std::string_view sw) {
#if MV_LOG_ENABLE_INF && !MV_LOG_DISABLED
    if (enabled(Level::Info)) {
        detail::write(Level::Info, sw);
    }
#endif  // MV_LOG_ENABLE_INF && !MV_LOG_DISABLED
}

inline void wrn([[maybe_unused]] const std::string_view sw) {
#if MV_LOG_ENABLE_WRN && !MV_LOG_DISABLED
    if (enabled(Level::Warning)) {
        detail::write(Level::Warning, sw);
    }
#endif  // MV_LOG_ENABLE_WRN && !MV_LOG_DISABLED
}

inline void err([[maybe_unused]] const std::string_view sw) {
#if MV_LOG_ENABLE_ERR && !MV_LOG_DISABLED
    if (enabled(Level::Error)) {
        detail::write(Level::Error, sw);
    }
#endif  // MV_LOG_ENABLE_ERR && !MV_LOG_DISABLED
}

template <typename... Args>    requires(sizeof...(Args) > 0)
inline void dbg([[maybe_unused]] std::format_string<Args...> format, [[maybe_unused]] Args&&... args) {
#if MV_LOG_ENABLE_DBG && !MV_LOG_DISABLED
    if (enabled(Level::Debug)) {
        detail::write(Level::Debug, std::format(format, std::forward<Args>(args)...));
    }
#endif  // MV_LOG_ENABLE_DBG && !MV_LOG_DISABLED
}

template <typename... Args>    requires(sizeof...(Args) > 0)
inline void inf([[maybe_unused]] std::format_string<Args...> format, [[maybe_unused]] Args&&... args) {
#if MV_LOG_ENABLE_INF && !MV_LOG_DISABLED
    if (enabled(Level::Info)) {
        detail::write(Level::Info, std::format(format, std::forward<Args>(args)...));
    }
#endif  // MV_LOG_ENABLE_INF && !MV_LOG_DISABLED
}

template <typename... Args>    requires(sizeof...(Args) > 0)
inline void wrn([[maybe_unused]] std::format_string<Args...> format, [[maybe_unused]] Args&&... args) {
#if MV_LOG_ENABLE_WRN && !MV_LOG_DISABLED
    if (enabled(Level::Warning)) {
        detail::write(Level::Warning, std::format(format, std::forward<Args>(args)...));
    }
#endif  // MV_LOG_ENABLE_WRN && !MV_LOG_DISABLED
}

template <typename... Args>    requires(sizeof...(Args) > 0)
inline void err([[maybe_unused]] std::format_string<Args...> format, [[maybe_unused]] Args&&... args) {
#if MV_LOG_ENABLE_ERR && !MV_LOG_DISABLED
    if (enabled(Level::Error)) {
        detail::write(Level::Error, std::format(format, std::forward<Args>(args)...));
    }
#endif  // MV_LOG_ENABLE_ERR && !MV_LOG_DISABLED
}
}  //  namespace mvlog

#if MV_LOG_DISABLED
inline mvlog::InitResult mvlog::init(const char* logfile_path, std::size_t max_size_kB, short max_files) {
    (void)logfile_path; (void)max_size_kB; (void)max_files;
    return InitResult::Ok;
}
#endif  // MV_LOG_DISABLED

#if !MV_LOG_DISABLED
#if MV_LOG_QT_TYPES_FORMAT
#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QStringList>
#include <QStringView>

template<>
struct std::formatter<QByteArray> : std::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const QByteArray& value, FormatContext& ctx) const {
        const std::string_view bytes(
            value.constData(),
            static_cast<std::size_t>(value.size())
            );
        return std::formatter<std::string_view>::format(bytes, ctx);
    }
};

template<>
struct std::formatter<QByteArrayView> : std::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(QByteArrayView value, FormatContext& ctx) const {
        const std::string_view bytes(
            value.constData(),
            static_cast<std::size_t>(value.size())
            );
        return std::formatter<std::string_view>::format(bytes, ctx);
    }
};

template<>
struct std::formatter<QStringView> : std::formatter<QByteArray> {
    template<typename FormatContext>
    auto format(QStringView value, FormatContext& ctx) const {
        const QByteArray utf8 = value.toUtf8();
        return std::formatter<QByteArray>::format(utf8, ctx);
    }
};

template<>
struct std::formatter<QString> : std::formatter<QStringView> {
    template<typename FormatContext>
    auto format(const QString& value, FormatContext& ctx) const {
        return std::formatter<QStringView>::format(QStringView(value), ctx);
    }
};

template<>
struct std::formatter<QStringList> : std::formatter<QStringView> {
    template<typename FormatContext>
    auto format(const QStringList& value, FormatContext& ctx) const {
        return std::formatter<QStringView>::format(QStringView(value.join(',')), ctx);
    }
};

#endif  // MV_LOG_QT_TYPES_FORMAT

namespace mvlog::detail {

bool utc_time(std::tm& date, std::time_t timestamp);
bool system_local_time(std::tm& date, std::time_t timestamp);
std::chrono::sys_seconds calendar_seconds(const std::tm& date);
void copy_time(char* destination, unsigned value) noexcept;

// One offset per UTC day, shared by all callers. DST/time-zone changes within
// that day are deliberately deferred until the next UTC day. A clock jump to
// another day also refreshes the cache, including jumps backwards.
// TZ/environment changes must not run concurrently with time conversion.
[[nodiscard]] bool local_time(std::tm& date, std::time_t timestamp);
[[nodiscard]] bool local_time(std::tm& date);
std::size_t estimate_record_size(std::size_t dynamic_size) noexcept;
std::string format_record(char level, const std::tm& date, std::string_view message);

class LocalOffsetCache {
public:
    [[nodiscard]] bool get(std::time_t timestamp, std::time_t& offset);

private:
    std::mutex _mutex;
    std::chrono::sys_days _utc_day{};
    std::time_t _offset{};
    bool _valid{false};
};


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

    [[nodiscard]] bool open(const std::filesystem::path& filename);
    [[nodiscard]] bool flush();
    [[nodiscard]] bool close() noexcept;
    [[nodiscard]] bool write(const char* buf, std::size_t bufsize);

    // Numeric suffixes reserve an index even for directories and symlinks.
    // On failure, max_index is unchanged. No directory recursion is needed.
    [[nodiscard]] static bool find_max_file_index(const std::filesystem::path& filename, std::uint64_t& max_index);

    // Caller serializes writes/rotation and exclusively owns this log family.
    // max_archives excludes the current file; 0 keeps all archives.
    // At the limit, keep the newest archives and renumber them from 1.
    // After an I/O failure, some archive moves may already have succeeded.
    [[nodiscard]] bool rotate(std::size_t max_archives = 0);

private:
    // Validate the full set before deleting or moving anything. Only canonical
    // numeric archives belonging to this log are touched; never follow links.
    [[nodiscard]] bool compact_archives(std::size_t keep);
};

}  // namespace mvlog::detail

#endif  // !MV_LOG_DISABLED

#if MV_LOG_QT_DISABLE_QDEBUG
#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#endif
