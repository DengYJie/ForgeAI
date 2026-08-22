#pragma once
#include <QString>
#include <QList>
#include <variant>
#include "domain/Types.h"
#include "domain/agent/ToolExecution.h"

namespace domain::conversation {
    /**
     * @brief 纯文本 / Markdown 消息载荷
     */
    struct TextBlock {
        QString text;
    };

    /**
     * @brief 思考链载荷（用于承载 DeepSeek R1 / OpenAI o1 等模型的推理思维流）
     */
    struct ThoughtBlock {
        QString thought; ///< 思考过程正文
        qint64 durationMs = 0; ///< 思考总耗时（毫秒）
    };

    /**
     * @brief 图片载荷
     */
    struct ImageBlock {
        QString urlOrLocalPath; ///< 本地缓存路径或远程 URL
        QString mimeType; ///< 图片 MIME 类型 (如 "image/png")
    };

    /**
     * @brief 工具调用指令载荷（模型下发）
     */
    struct ToolCallBlock {
        QList<domain::agent::ToolCall> calls; ///< 包含的工具调用列表（可能同时并发调用多个工具）
    };

    /**
     * @brief 工具执行结果载荷（本地返回）
     */
    struct ToolResultBlock {
        QList<domain::agent::ToolResult> results; ///< 对应工具调用的执行结果列表
    };

    /**
     * @brief 消息块组合结构（采用 std::variant 实现类型安全的多态载荷）
     */
    struct MessageBlock {
        domain::BlockType type; ///< 消息块类型枚举

        using Payload = std::variant<TextBlock, ThoughtBlock, ImageBlock, ToolCallBlock, ToolResultBlock>;
        Payload payload; ///< 具体载荷内容

        bool isText() const { return std::holds_alternative<TextBlock>(payload); }
        bool isThought() const { return std::holds_alternative<ThoughtBlock>(payload); }
        bool isImage() const { return std::holds_alternative<ImageBlock>(payload); }
        bool isToolCall() const { return std::holds_alternative<ToolCallBlock>(payload); }
        bool isToolResult() const { return std::holds_alternative<ToolResultBlock>(payload); }
    };
} // namespace domain::conversation
