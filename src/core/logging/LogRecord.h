#pragma once
#include <QString>
#include <QDateTime>
#include <QMap>
#include <memory>
#include "LogLevel.h"
#include "LogContext.h"

namespace core::logging {

    /**
     * @brief 日志记录项
     */
    struct LogRecord {
        qint64 timestamp = 0;
        LogLevel level = LogLevel::Info;
        QString category;
        QString message;
        
        std::shared_ptr<const LogContext> context;
        QMap<QString, QString> fields;

        LogRecord()
            : timestamp(QDateTime::currentMSecsSinceEpoch()) {}

        LogRecord(
            LogLevel lvl,
            const QString &cat,
            const QString &msg,
            const QMap<QString, QString> &f = {},
            std::shared_ptr<const LogContext> ctx = nullptr)
            : timestamp(QDateTime::currentMSecsSinceEpoch())
            , level(lvl)
            , category(cat)
            , message(msg)
            , context(std::move(ctx))
            , fields(f) {}
    };

} // namespace core::logging
