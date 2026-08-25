#pragma once

#include <QObject>
#include <QJsonObject>
#include <QString>

namespace llm::mcp {

    /**
     * @brief MCP 底层传输通道抽象接口
     */
    class IMcpTransport : public QObject {
        Q_OBJECT
    public:
        using QObject::QObject;
        ~IMcpTransport() override = default;

        virtual bool start() = 0;
        virtual void close() = 0;
        virtual bool sendJson(const QJsonObject& json) = 0;
        virtual bool isConnected() const = 0;

    Q_SIGNALS:
        void messageReceived(const QJsonObject& message);
        void errorOccurred(const QString& error);
        void closed();
    };

} // namespace llm::mcp
