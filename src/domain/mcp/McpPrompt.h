#pragma once

#include <QString>
#include <QList>

namespace domain::mcp {

    /**
     * @brief MCP Prompt 参数描述
     */
    struct McpPromptArgument {
        QString name;
        QString description;
        bool required = false;
    };

    /**
     * @brief MCP Prompt 提示模板定义
     */
    struct McpPrompt {
        QString name;
        QString description;
        QList<McpPromptArgument> arguments;
    };

    /**
     * @brief MCP Prompt 渲染生成的消息内容
     */
    struct McpPromptMessage {
        QString role;           ///< user / assistant
        QString content;        ///< 文本内容
    };

} // namespace domain::mcp
