#pragma once

#include <QString>
#include <QJsonObject>

namespace domain::mcp {

    /**
     * @brief MCP 暴露的静态/动态资源定义 (Resource)
     */
    struct McpResource {
        QString uri;            ///< 资源 URI 标识 (例如 "file:///path/to/file")
        QString name;           ///< 资源名称
        QString description;    ///< 资源描述
        QString mimeType;       ///< MIME 类型 (例如 "text/plain", "application/json")
    };

    /**
     * @brief MCP 暴露的资源内容
     */
    struct McpResourceContent {
        QString uri;
        QString mimeType;
        QString text;           ///< 文本内容 (与 blob 二选一)
        QByteArray blob;        ///< 二进制内容
    };

} // namespace domain::mcp
