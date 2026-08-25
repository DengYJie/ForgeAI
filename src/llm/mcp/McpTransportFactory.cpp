#include "McpTransportFactory.h"
#include "StdioMcpTransport.h"
#include "StreamableHttpMcpTransport.h"

namespace llm::mcp {

    std::unique_ptr<IMcpTransport> McpTransportFactory::create(
        const domain::mcp::McpServerConfig& config,
        QObject* parent
    ) const {
        if (config.transport == domain::mcp::McpTransportType::Http || (!config.url.isEmpty() && config.command.isEmpty())) {
            return std::make_unique<StreamableHttpMcpTransport>(config, parent);
        }

        if (config.transport == domain::mcp::McpTransportType::Stdio || !config.command.isEmpty()) {
            return std::make_unique<StdioMcpTransport>(config, parent);
        }

        return nullptr;
    }

} // namespace llm::mcp
