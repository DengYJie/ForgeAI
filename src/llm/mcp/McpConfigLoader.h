#pragma once

#include <QString>
#include <QList>
#include <QByteArray>
#include "domain/mcp/McpServerConfig.h"

namespace llm::mcp {

    /**
     * @brief MCP 配置解析单项诊断信息
     */
    struct McpConfigIssue {
        QString serverId;
        QString message;
        bool isFatal = false;
    };

    /**
     * @brief MCP 配置文件加载与解析结果
     */
    struct McpConfigLoadResult {
        bool success = true;
        QString error;
        QList<McpConfigIssue> issues;
        QList<domain::mcp::McpServerConfig> configs;
    };

    /**
     * @brief MCP 配置文件解析器 (专职负责 .mcp.json / mcp.json 读取与合法性校验)
     */
    class McpConfigLoader {
    public:
        McpConfigLoader() = default;

        /**
         * @brief 从指定文件路径加载 MCP 服务配置
         * @param filePath 配置文件完整路径
         * @return 加载与解析结果
         */
        static McpConfigLoadResult loadFromFile(const QString& filePath);

        /**
         * @brief 从 JSON 字节流解析 MCP 服务配置
         * @param jsonData JSON 字节数组
         * @param baseDir 用于解析相对路径的基础目录 (可选)
         * @return 加载与解析结果
         */
        static McpConfigLoadResult loadFromJson(const QByteArray& jsonData, const QString& baseDir = {});

        /**
         * @brief 从 JSON 字符串解析 MCP 服务配置
         */
        static McpConfigLoadResult loadFromJsonString(const QString& jsonString, const QString& baseDir = {});
    };

} // namespace llm::mcp
