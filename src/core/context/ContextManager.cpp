#include "ContextManager.h"
#include <algorithm>

namespace core::context {
    // 辅助函数：针对超长工具返回进行 Head-Tail 截断（参考 OpenCode 最佳实践）
    static QString clampToolOutput(const QString &rawOutput, int maxChars = 8000) {
        if (rawOutput.length() <= maxChars) {
            return rawOutput;
        }
        int headLen = maxChars / 3;
        int tailLen = (maxChars * 2) / 3;
        int omitted = rawOutput.length() - headLen - tailLen;
        return rawOutput.left(headLen) +
               QString("\n\n[... %1 characters omitted for context limit ...]\n\n").arg(omitted) +
               rawOutput.right(tailLen);
    }

    int ContextManager::estimateTokens(const QString &text) const {
        if (text.isEmpty()) return 0;

        // 工业级轻量混合分词估算（安全系数加权）
        int tokens = 0;
        for (const QChar &ch: text) {
            if (ch.unicode() > 127) {
                tokens += 2; // CJK 字符 / Emoji 约 1.5-2 tokens
            } else {
                tokens += 1;
            }
        }
        return std::max(1, tokens / 2);
    }

    int ContextManager::estimateMessageTokens(const domain::conversation::Message &msg) const {
        int tokens = 4; // 基础 Envelope / Role 开销
        for (const auto &block: msg.blocks) {
            std::visit([this, &tokens](auto &&payload) {
                using T = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<T, domain::conversation::TextBlock>) {
                    tokens += estimateTokens(payload.text);
                } else if constexpr (std::is_same_v<T, domain::conversation::ThoughtBlock>) {
                    tokens += estimateTokens(payload.thought);
                } else if constexpr (std::is_same_v<T, domain::conversation::ImageBlock>) {
                    tokens += 85;
                } else if constexpr (std::is_same_v<T, domain::conversation::ToolCallBlock>) {
                    for (const auto &call: payload.calls) {
                        tokens += estimateTokens(call.name) + estimateTokens(call.arguments) + 10;
                    }
                } else if constexpr (std::is_same_v<T, domain::conversation::ToolResultBlock>) {
                    for (const auto &res: payload.results) {
                        // 工具输出受 clampToolOutput 保护
                        tokens += estimateTokens(clampToolOutput(res.content)) + 10;
                    }
                }
            }, block.payload);
        }
        return tokens;
    }

    AssembledContext ContextManager::assemble(
        const domain::conversation::Conversation &conversation,
        const std::optional<domain::agent::Agent> &agent,
        const QList<domain::conversation::Message> &fullHistory,
        const QList<domain::agent::ToolDefinition> &availableTools,
        const QStringList &availableSkillSummaries,
        int contextBudgetTokens,
        int reservedOutputTokens
    ) {
        AssembledContext result;
        result.tools = availableTools;

        // 1. 组装 System Prompt (参考 oh-my-pi：人设 + 紧凑的 Skill 索引)
        QStringList systemParts;
        if (agent.has_value() && !agent->systemPrompt.isEmpty()) {
            systemParts.append(agent->systemPrompt);
        } else {
            systemParts.append("You are a helpful and versatile AI assistant.");
        }

        if (!availableSkillSummaries.isEmpty()) {
            systemParts.append("## Available Skills (Load on demand):\n" + availableSkillSummaries.join("\n"));
        }

        result.systemPrompt = systemParts.join("\n\n");
        int systemTokens = estimateTokens(result.systemPrompt);

        // 估算 Tools 声明开销
        int toolsTokens = 0;
        for (const auto &tool: availableTools) {
            toolsTokens += estimateTokens(tool.name) + estimateTokens(tool.description) + 20;
        }

        // 2. 计算安全预算（引入 10% 安全缓冲区 Safety Margin，防止 Token 估算溢出）
        int safeTotalBudget = static_cast<int>(contextBudgetTokens * 0.90);
        int availableHistoryBudget = safeTotalBudget - reservedOutputTokens - systemTokens - toolsTokens;
        if (availableHistoryBudget < 1000) {
            availableHistoryBudget = 1000; // 兜底至少容纳 1 轮完整交互
        }

        // 3. 按 Turn 进行原子分组逆序装配 (Turn-Level Atomic Grouping)
        // 先将 fullHistory 按 turnId 分组，杜绝 ToolCall 与 ToolResult 孤儿断裂！
        QList<QList<domain::conversation::Message> > turnGroups;
        for (const auto &msg: fullHistory) {
            if (turnGroups.isEmpty() || turnGroups.last().first().turnId != msg.turnId) {
                turnGroups.append(QList<domain::conversation::Message>{msg});
            } else {
                turnGroups.last().append(msg);
            }
        }

        QList<domain::conversation::Message> selectedHistory;
        int currentHistoryTokens = 0;

        // 从最新的 Turn 往前回溯装填
        for (int i = turnGroups.size() - 1; i >= 0; --i) {
            const auto &turnMsgs = turnGroups.at(i);
            int turnTokens = 0;

            // 处理每条消息（应用 Tool Output Clamping 并计算该 Turn 总 Token）
            QList<domain::conversation::Message> clampedTurnMsgs = turnMsgs;
            for (auto &msg: clampedTurnMsgs) {
                for (auto &block: msg.blocks) {
                    if (block.type == domain::BlockType::ToolResult) {
                        if (auto *resBlock = std::get_if<domain::conversation::ToolResultBlock>(&block.payload)) {
                            for (auto &res: resBlock->results) {
                                res.content = clampToolOutput(res.content);
                            }
                        }
                    }
                }
                turnTokens += estimateMessageTokens(msg);
            }

            // 超出预算则停止
            if (currentHistoryTokens + turnTokens > availableHistoryBudget && !selectedHistory.isEmpty()) {
                break;
            }

            // 将整轮 Turn 原子性地插入到头部
            for (int j = clampedTurnMsgs.size() - 1; j >= 0; --j) {
                selectedHistory.prepend(clampedTurnMsgs.at(j));
            }
            currentHistoryTokens += turnTokens;
        }

        // 4. 角色清洗（Role Sanitization，确保裁剪后第一条历史记录必须是 User，兼容 Claude 规范）
        while (!selectedHistory.isEmpty() && selectedHistory.first().role != domain::MessageRole::User) {
            // 如果截断后最老的消息是 Assistant 或 Tool，顺延丢弃，直到找到 User 消息
            currentHistoryTokens -= estimateMessageTokens(selectedHistory.first());
            selectedHistory.removeFirst();
        }

        result.history = selectedHistory;
        result.estimatedTokens = systemTokens + toolsTokens + currentHistoryTokens;

        return result;
    }
} // namespace core::context
