#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

namespace llm::mcp {

    /**
     * @brief MCP (Model Context Protocol) 外部服务器配置
     */
    struct McpServerConfig {
        QString name;                   ///< 服务唯一标识名
        QString command;                ///< 可执行命令 (如 npx, python, docker)
        QStringList args;               ///< 命令行参数
        QMap<QString, QString> env;     ///< 环境变量
        QString cwd;                    ///< 工作目录
        bool disabled = false;          ///< 是否禁用
        bool autoApprove = false;       ///< 是否自动授权执行

        bool operator==(const McpServerConfig &other) const = default;
    };

} // namespace llm::mcp
