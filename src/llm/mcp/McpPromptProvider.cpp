#include "McpPromptProvider.h"
#include "McpRuntime.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"
#include <QElapsedTimer>

namespace llm::mcp {

    McpPromptProvider::McpPromptProvider(McpRuntime* runtime, QObject* parent)
        : QObject(parent), m_runtime(runtime) {
    }

    QList<domain::mcp::McpPrompt> McpPromptProvider::prompts() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<domain::mcp::McpPrompt> list;
        if (!m_runtime) return list;

        for (auto* session : m_runtime->allSessions()) {
            if (session && session->state() == domain::mcp::McpConnectionState::Ready && session->client()) {
                list.append(session->client()->listPrompts());
            }
        }
        return list;
    }

    QList<domain::mcp::McpPromptMessage> McpPromptProvider::getPrompt(
        const QString& name,
        const QJsonObject& arguments,
        int timeoutMs
    ) {
        QElapsedTimer timer;
        timer.start();

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_runtime) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP Prompt 获取失败: 运行时未就绪"), {
                {QStringLiteral("promptName"), name}
            });
            return {};
        }

        for (auto* session : m_runtime->allSessions()) {
            if (session && session->state() == domain::mcp::McpConnectionState::Ready && session->client()) {
                auto messages = session->client()->getPrompt(name, arguments, timeoutMs);
                if (!messages.isEmpty()) {
                    core::logging::LoggingService::instance().debug(core::logging::Category::McpProtocol, QStringLiteral("MCP Prompt 获取成功"), {
                        {QStringLiteral("promptName"), name},
                        {QStringLiteral("messageCount"), QString::number(messages.size())},
                        {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
                    });
                    return messages;
                }
            }
        }

        core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP Prompt 获取失败或未找到提示词"), {
            {QStringLiteral("promptName"), name},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });
        return {};
    }

} // namespace llm::mcp
