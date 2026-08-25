#include "McpResourceProvider.h"
#include "McpRuntime.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"
#include "core/logging/SensitiveDataFilter.h"
#include <QElapsedTimer>

namespace llm::mcp {

    McpResourceProvider::McpResourceProvider(McpRuntime* runtime, QObject* parent)
        : QObject(parent), m_runtime(runtime) {
    }

    QList<domain::mcp::McpResource> McpResourceProvider::resources() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<domain::mcp::McpResource> list;
        if (!m_runtime) return list;

        for (auto* session : m_runtime->allSessions()) {
            if (session && session->state() == domain::mcp::McpConnectionState::Ready && session->client()) {
                list.append(session->client()->listResources());
            }
        }
        return list;
    }

    std::optional<domain::mcp::McpResourceContent> McpResourceProvider::readResource(const QString& uri, int timeoutMs) {
        QElapsedTimer timer;
        timer.start();

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_runtime) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP Resource 读取失败: 运行时未就绪"), {
                {QStringLiteral("uri"), core::logging::SensitiveDataFilter::sanitizeUrl(uri)}
            });
            return std::nullopt;
        }

        for (auto* session : m_runtime->allSessions()) {
            if (session && session->state() == domain::mcp::McpConnectionState::Ready && session->client()) {
                auto content = session->client()->readResource(uri, timeoutMs);
                if (content.has_value()) {
                    core::logging::LoggingService::instance().debug(core::logging::Category::McpProtocol, QStringLiteral("MCP Resource 读取成功"), {
                        {QStringLiteral("uri"), core::logging::SensitiveDataFilter::sanitizeUrl(uri)},
                        {QStringLiteral("bytes"), QString::number(content->text.toUtf8().size() + content->blob.size())},
                        {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
                    });
                    return content;
                }
            }
        }

        core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP Resource 读取失败或未找到资源"), {
            {QStringLiteral("uri"), core::logging::SensitiveDataFilter::sanitizeUrl(uri)},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });
        return std::nullopt;
    }

} // namespace llm::mcp
