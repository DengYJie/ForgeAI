#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <optional>
#include "domain/conversation/Conversation.h"
#include "domain/conversation/Message.h"
#include "domain/agent/Agent.h"
#include "domain/agent/ToolDefinition.h"

namespace core::context {
    /**
     * @brief 装配完成后的上下文载荷（直接提供给 ModelProvider 进行 API 请求）
     */
    struct AssembledContext {
        QString systemPrompt; ///< 组装好人设、基础环境约束与按需 Skill 索引的系统提示词
        QList<domain::conversation::Message> history; ///< 经过严格 Token 预算与原子 Turn 裁剪后的历史消息流
        QList<domain::agent::ToolDefinition> tools; ///< 当前激活的内置 C++ 工具与外部 MCP 工具列表
        int estimatedTokens = 0; ///< 本次请求预估消耗的总 Token 数
    };

    /**
     * @brief 上下文与 Token 预算管理器
     * @details 负责智能裁剪历史、防止 ToolCall 孤儿断裂、Head-Tail 输出截断及系统提示词装配。
     */
    class ContextManager {
    public:
        ContextManager() = default;

        /**
         * @brief 根据预算动态装配发送给大模型的完整上下文
         * @param conversation 当前会话元数据
         * @param agent 智能体配置（可选）
         * @param fullHistory 从仓储读取的完整消息列表
         * @param availableTools 当前可用的工具定义列表
         * @param availableSkillSummaries 已注册 Skill 的简述索引列表
         * @param contextBudgetTokens 上下文 Token 总预算上限 (默认 32k)
         * @param reservedOutputTokens 预留给模型生成回答的最小 Token 空间 (默认 4k)
         * @return 装配好的 AssembledContext 结构
         */
        AssembledContext assemble(
            const domain::conversation::Conversation &conversation,
            const std::optional<domain::agent::Agent> &agent,
            const QList<domain::conversation::Message> &fullHistory,
            const QList<domain::agent::ToolDefinition> &availableTools,
            const QStringList &availableSkillSummaries,
            int contextBudgetTokens = 32768,
            int reservedOutputTokens = 4096
        );

        /**
         * @brief 估算一段纯文本的 Token 消耗
         */
        int estimateTokens(const QString &text) const;

        /**
         * @brief 估算单条消息（包含所有多态 MessageBlock）的 Token 消耗
         */
        int estimateMessageTokens(const domain::conversation::Message &msg) const;
    };
} // namespace core::context
