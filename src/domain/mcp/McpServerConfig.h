#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include "domain/mcp/McpTransportType.h"

namespace domain::mcp {

    /**
     * @brief MCP 外部服务端静态配置实体
     */
    struct McpServerConfig {
        QString id;                                             ///< 服务唯一标识 (例如 "github", "filesystem")
        QString name;                                           ///< 服务显示名称 (默认同 id)
        McpTransportType transport = McpTransportType::Stdio;   ///< 传输协议

        // Stdio 配置
        QString command;                                        ///< 可执行命令 (如 npx, python, uvx)
        QStringList args;                                       ///< 命令参数
        QMap<QString, QString> env;                             ///< 环境变量
        QString cwd;                                            ///< 工作目录

        // HTTP / SSE / WebSocket 配置
        QString url;                                            ///< 远程服务 URL
        QMap<QString, QString> headers;                         ///< 自定义 HTTP 请求头

        bool enabled = true;                                    ///< 是否启用
        bool disabled = false;                                  ///< 兼容字段
        bool autoApprove = false;                               ///< 是否自动授权执行

        bool isEnabled() const {
            return enabled && !disabled;
        }

        bool operator==(const McpServerConfig &other) const = default;
    };

} // namespace domain::mcp
