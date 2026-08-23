#pragma once
#include <QString>
#include <QMap>
#include <memory>
#include "LogLevel.h"
#include "LogContext.h"

namespace core::logging {

    /**
     * @brief 通用日志记录接口
     */
    class ILogger {
    public:
        virtual ~ILogger() = default;

        virtual LogLevel minLevel() const = 0;

        inline bool isEnabled(LogLevel level) const {
            return static_cast<uint8_t>(level) >= static_cast<uint8_t>(minLevel());
        }

        virtual void log(
            LogLevel level,
            const QString &category,
            const QString &message,
            const QMap<QString, QString> &fields = {},
            std::shared_ptr<const LogContext> context = nullptr) = 0;

        inline void trace(const QString &category, const QString &message, const QMap<QString, QString> &fields = {}, std::shared_ptr<const LogContext> context = nullptr) {
            if (isEnabled(LogLevel::Trace)) log(LogLevel::Trace, category, message, fields, std::move(context));
        }

        inline void debug(const QString &category, const QString &message, const QMap<QString, QString> &fields = {}, std::shared_ptr<const LogContext> context = nullptr) {
            if (isEnabled(LogLevel::Debug)) log(LogLevel::Debug, category, message, fields, std::move(context));
        }

        inline void info(const QString &category, const QString &message, const QMap<QString, QString> &fields = {}, std::shared_ptr<const LogContext> context = nullptr) {
            if (isEnabled(LogLevel::Info)) log(LogLevel::Info, category, message, fields, std::move(context));
        }

        inline void warning(const QString &category, const QString &message, const QMap<QString, QString> &fields = {}, std::shared_ptr<const LogContext> context = nullptr) {
            if (isEnabled(LogLevel::Warning)) log(LogLevel::Warning, category, message, fields, std::move(context));
        }

        inline void error(const QString &category, const QString &message, const QMap<QString, QString> &fields = {}, std::shared_ptr<const LogContext> context = nullptr) {
            if (isEnabled(LogLevel::Error)) log(LogLevel::Error, category, message, fields, std::move(context));
        }

        inline void critical(const QString &category, const QString &message, const QMap<QString, QString> &fields = {}, std::shared_ptr<const LogContext> context = nullptr) {
            if (isEnabled(LogLevel::Critical)) log(LogLevel::Critical, category, message, fields, std::move(context));
        }
    };

} // namespace core::logging
