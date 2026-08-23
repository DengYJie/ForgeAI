#pragma once
#include <QString>
#include <cstdint>

namespace core::logging {

    /**
     * @brief 标准日志级别 (8-bit enum for low memory overhead)
     */
    enum class LogLevel : uint8_t {
        Trace = 0,      ///< 细粒度协议与解析器调试
        Debug = 1,      ///< 开发诊断信息
        Info = 2,       ///< 关键业务生命周期事件
        Warning = 3,    ///< 异常或重试警告
        Error = 4,      ///< 操作或请求失败
        Critical = 5    ///< 系统级致命错误
    };

    inline QString logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Trace:    return QStringLiteral("TRACE");
            case LogLevel::Debug:    return QStringLiteral("DEBUG");
            case LogLevel::Info:     return QStringLiteral("INFO");
            case LogLevel::Warning:  return QStringLiteral("WARN");
            case LogLevel::Error:    return QStringLiteral("ERROR");
            case LogLevel::Critical: return QStringLiteral("CRIT");
        }
        return QStringLiteral("INFO");
    }

    inline LogLevel stringToLogLevel(const QString &str, LogLevel defaultLevel = LogLevel::Info) {
        QString upper = str.toUpper().trimmed();
        if (upper == "TRACE") return LogLevel::Trace;
        if (upper == "DEBUG") return LogLevel::Debug;
        if (upper == "INFO")  return LogLevel::Info;
        if (upper == "WARN" || upper == "WARNING") return LogLevel::Warning;
        if (upper == "ERROR") return LogLevel::Error;
        if (upper == "CRIT" || upper == "CRITICAL") return LogLevel::Critical;
        return defaultLevel;
    }

} // namespace core::logging
