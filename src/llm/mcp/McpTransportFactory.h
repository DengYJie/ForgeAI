#pragma once

#include <memory>
#include <QObject>
#include "domain/mcp/McpServerConfig.h"
#include "IMcpTransport.h"

namespace llm::mcp {

    /**
     * @brief MCP 传输通道工厂（负责根据配置类型实例化具体 Transport 实现，解耦 Session 与具体通道类）
     */
    class McpTransportFactory {
    public:
        virtual ~McpTransportFactory() = default;

        /**
         * @brief 根据配置创建对应的 Transport 实例
         */
        virtual std::unique_ptr<IMcpTransport> create(
            const domain::mcp::McpServerConfig& config,
            QObject* parent = nullptr
        ) const;
    };

} // namespace llm::mcp
