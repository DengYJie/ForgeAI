#pragma once
#include <QString>
#include <QList>
#include <optional>
#include "domain/Types.h"
#include "domain/agent/ToolExecution.h"

namespace domain::llm {

    /**
     * @brief 面向 LLM 网络协议层的纯粹消息数据传输对象 (DTO)
     * @details 剥离了业务域中复杂的 ID、时间戳、持久化状态。
     *          初期仅支持纯文本，未来通过扩展 ContentPart 数组支持多模态。
     */
    struct ChatMessage {
        domain::MessageRole role = domain::MessageRole::User; ///< 角色：System, User, Assistant, Tool
        QString content; ///< 消息文本正文

        // 工具调用与结果字段
        QString name; ///< Tool 的名称或调用函数名
        QString toolCallId; ///< Tool 调用的关联 ID
        QString reasoningContent; ///< Assistant 消息的深度思考链内容 (如 DeepSeek/Qwen 的 reasoning_content)
        std::optional<QList<domain::agent::ToolCall>> toolCalls; ///< Assistant 消息包含的工具调用指令

        bool operator==(const ChatMessage &other) const = default;
    };

} // namespace domain::llm
