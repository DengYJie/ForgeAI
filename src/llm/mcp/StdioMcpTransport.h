#pragma once

#include "IMcpTransport.h"
#include "McpServerConfig.h"
#include <QProcess>
#include <QByteArray>

namespace llm::mcp {

    /**
     * @brief 基于标准输入输出管道的子进程 MCP 传输通道
     */
    class StdioMcpTransport final : public IMcpTransport {
        Q_OBJECT
    public:
        explicit StdioMcpTransport(const McpServerConfig& config, QObject* parent = nullptr);
        ~StdioMcpTransport() override;

        bool start() override;
        void close() override;
        bool sendJson(const QJsonObject& json) override;
        bool isConnected() const override;

    private Q_SLOTS:
        void onReadyReadStandardOutput();
        void onReadyReadStandardError();
        void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
        void onProcessError(QProcess::ProcessError error);

    private:
        McpServerConfig m_config;
        QProcess* m_process = nullptr;
        QByteArray m_readBuffer;
    };

} // namespace llm::mcp
