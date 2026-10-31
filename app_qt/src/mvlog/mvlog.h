#pragma once

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

#include <cstdint>
#include <cstddef>
#include <format>
#include <string_view>
#include <utility>


namespace mvlog {
enum class InitResult : std::int8_t { Ok, InvalidArgument, AlreadyInitialized, FileError };
// UTF-8 path supplied by the app (use its private data directory on Android); size is in KiB, 0 disables size-based rotation.
// max_files limits archives (excluding the current file); 0 retains them all.
// Initialization is allowed once; failure leaves console/Logcat output available.
[[nodiscard]] InitResult init(const char* logfile_path, std::size_t max_size_kB, short max_files);
enum class Level : char { Debug = 'D', Info = 'I', Warning = 'W', Error = 'E' };

namespace detail {
void write(const Level lvl, const std::string_view message);
}  // namespace detail

inline void dbg(const std::string_view sw) {
#if MV_LOG_ENABLE_DBG && !MV_LOG_DISABLED
    detail::write(Level::Debug, sw);
#endif
}

inline void inf(const std::string_view sw) {
#if MV_LOG_ENABLE_INF && !MV_LOG_DISABLED
    detail::write(Level::Info, sw);
#endif
}

inline void wrn(const std::string_view sw) {
#if MV_LOG_ENABLE_WRN && !MV_LOG_DISABLED
    detail::write(Level::Warning, sw);
#endif
}

inline void err(const std::string_view sw) {
#if MV_LOG_ENABLE_ERR && !MV_LOG_DISABLED
    detail::write(Level::Error, sw);
#endif
}

template <typename... Args>    requires(sizeof...(Args) > 0)
inline void dbg(std::format_string<Args...> format, Args&&... args) {
#if MV_LOG_ENABLE_DBG && !MV_LOG_DISABLED
    dbg(std::format(format, std::forward<Args>(args)...));
#endif
}

template <typename... Args>    requires(sizeof...(Args) > 0)
inline void inf(std::format_string<Args...> format, Args&&... args) {
#if MV_LOG_ENABLE_INF && !MV_LOG_DISABLED
    inf(std::format(format, std::forward<Args>(args)...));
#endif
}

template <typename... Args>    requires(sizeof...(Args) > 0)
inline void wrn(std::format_string<Args...> format, Args&&... args) {
#if MV_LOG_ENABLE_WRN && !MV_LOG_DISABLED
    wrn(std::format(format, std::forward<Args>(args)...));
#endif
}

template <typename... Args>    requires(sizeof...(Args) > 0)
inline void err(std::format_string<Args...> format, Args&&... args) {
#if MV_LOG_ENABLE_ERR && !MV_LOG_DISABLED
    err(std::format(format, std::forward<Args>(args)...));
#endif
}
}  // namespace mvlog

#if MV_LOG_DISABLED
inline mvlog::InitResult mvlog::init(const char* logfile_path, std::size_t max_size_kB, short max_files) {
    return InitResult::Ok;
}
#endif

#if MV_LOG_QT_TYPES_FORMAT && !MV_LOG_DISABLED
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
#endif

#if MV_LOG_QT_DISABLE_QDEBUG
#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#endif
