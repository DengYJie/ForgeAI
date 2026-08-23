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

        bool operator==(const TextBlock &other) const = default;
    };

    /**
     * @brief 思考链载荷（用于承载大语言模型的推理思维流）
     */
    struct ThoughtBlock {
        QString thought; ///< 思考过程正文
        qint64 durationMs = 0; ///< 思考总耗时（毫秒）

        bool operator==(const ThoughtBlock &other) const = default;
    };

    /**
     * @brief 图片载荷
     */
    struct ImageBlock {
        QString urlOrLocalPath; ///< 本地缓存路径或远程 URL
        QString mimeType; ///< 图片 MIME 类型 (如 "image/png")

        bool operator==(const ImageBlock &other) const = default;
    };

    /**
     * @brief 工具调用指令载荷（模型下发）
     */
    struct ToolCallBlock {
        QList<domain::agent::ToolCall> calls; ///< 包含的工具调用列表（可能同时并发调用多个工具）

        bool operator==(const ToolCallBlock &other) const = default;
    };

    /**
     * @brief 工具执行结果载荷（本地返回）
     */
    struct ToolResultBlock {
        QList<domain::agent::ToolResult> results; ///< 对应工具调用的执行结果列表

        bool operator==(const ToolResultBlock &other) const = default;
    };

    /**
     * @brief 消息块组合结构（采用 std::variant 实现类型安全的多态载荷）
     */
    struct MessageBlock {
        using Payload = std::variant<TextBlock, ThoughtBlock, ImageBlock, ToolCallBlock, ToolResultBlock>;

        domain::BlockType type = domain::BlockType::Text; ///< 消息块类型枚举
        Payload payload = TextBlock{}; ///< 具体载荷内容

        MessageBlock() = default;
        MessageBlock(domain::BlockType t, Payload p) : type(t), payload(std::move(p)) {}

        bool isText() const { return std::holds_alternative<TextBlock>(payload); }
        bool isThought() const { return std::holds_alternative<ThoughtBlock>(payload); }
        bool isImage() const { return std::holds_alternative<ImageBlock>(payload); }
        bool isToolCall() const { return std::holds_alternative<ToolCallBlock>(payload); }
        bool isToolResult() const { return std::holds_alternative<ToolResultBlock>(payload); }

        bool operator==(const MessageBlock &other) const = default;
    };
} // namespace domain::conversation
