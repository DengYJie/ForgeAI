#pragma once
#include <memory>
#include <QMap>
#include <QString>

namespace core::logging {

    /**
     * @brief 日志上下文
     */
    struct LogContext {
        QString sessionId;
        QString operationId;
        QString requestId;
        QString providerId;
        QString modelId;
        QString conversationId;
        QMap<QString, QString> fields;

        static std::shared_ptr<LogContext> create() {
            return std::make_shared<LogContext>();
        }

        std::shared_ptr<LogContext> cloneWithRequestId(const QString& reqId) const {
            auto copy = std::make_shared<LogContext>(*this);
            copy->requestId = reqId;
            return copy;
        }

        std::shared_ptr<LogContext> cloneWithOperationId(const QString& opId) const {
            auto copy = std::make_shared<LogContext>(*this);
            copy->operationId = opId;
            return copy;
        }
    };

} // namespace core::logging
