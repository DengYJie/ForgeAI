#pragma once
#include <QString>
#include <QDateTime>
#include "LogRecord.h"
#include "SensitiveDataFilter.h"

namespace core::logging {

    class LogFormatter {
    public:
        /**
         * @brief 将结构化 LogRecord 格式化为单行文本（在后台工作线程执行，不占用业务线程时间）
         */
        static QString format(const LogRecord &record, bool colorize = false) {
            QString timeStr = QDateTime::fromMSecsSinceEpoch(record.timestamp).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
            QString levelStr = logLevelToString(record.level);
            
            if (colorize) {
                levelStr = colorizeLevel(record.level, levelStr);
            }

            QString categoryStr = record.category.isEmpty() ? QStringLiteral("root") : record.category;

            QString result = QStringLiteral("%1 %2 [%3]").arg(timeStr, levelStr, categoryStr);

            if (record.context) {
                if (!record.context->sessionId.isEmpty()) result += QStringLiteral(" ses=%1").arg(record.context->sessionId);
                if (!record.context->operationId.isEmpty()) result += QStringLiteral(" op=%1").arg(record.context->operationId);
                if (!record.context->requestId.isEmpty()) result += QStringLiteral(" req=%1").arg(record.context->requestId);
                if (!record.context->providerId.isEmpty()) result += QStringLiteral(" provider=%1").arg(record.context->providerId);
                if (!record.context->modelId.isEmpty()) result += QStringLiteral(" model=%1").arg(record.context->modelId);
                
                for (auto it = record.context->fields.constBegin(); it != record.context->fields.constEnd(); ++it) {
                    result += QStringLiteral(" %1=%2").arg(it.key(), it.value());
                }
            }

            for (auto it = record.fields.constBegin(); it != record.fields.constEnd(); ++it) {
                result += QStringLiteral(" %1=%2").arg(it.key(), it.value());
            }

            if (!record.message.isEmpty()) {
                QString sanitizedMsg = SensitiveDataFilter::redactText(record.message);
                result += QStringLiteral(" - %1").arg(sanitizedMsg);
            }

            return result;
        }

    private:
        static QString colorizeLevel(LogLevel level, const QString &text) {
            switch (level) {
                case LogLevel::Trace:    return QStringLiteral("\033[90m%1\033[0m").arg(text);
                case LogLevel::Debug:    return QStringLiteral("\033[36m%1\033[0m").arg(text);
                case LogLevel::Info:     return QStringLiteral("\033[32m%1\033[0m").arg(text);
                case LogLevel::Warning:  return QStringLiteral("\033[33m%1\033[0m").arg(text);
                case LogLevel::Error:    return QStringLiteral("\033[31m%1\033[0m").arg(text);
                case LogLevel::Critical: return QStringLiteral("\033[1;31m%1\033[0m").arg(text);
            }
            return text;
        }
    };

} // namespace core::logging
