#pragma once
#include <QString>
#include <QJsonObject>

namespace domain::agent {
    /**
     * @brief 工具定义（遵循 OpenAI / MCP 的 Function Calling 声明规范）
     */
    struct ToolDefinition {
        QString name; ///< 工具名称 (如 "read_file", "search_web")
        QString description; ///< 工具功能描述（大模型据此推断调用时机）
        QJsonObject parameters; ///< 参数规范（遵循 JSON Schema 格式）
    };
} // namespace domain::agent
