#pragma once

#include <QString>
#include <QList>
#include "agent/runtime/AgentRunContext.h"
#include "domain/project/ProjectContext.h"
#include "domain/agent/Skill.h"

namespace agent::runtime {

    /**
     * @brief Agent 上下文与 System Prompt 统一装配器
     * @details 集中收口 System Prompt、AGENTS.md、Skills 指令与工作区元数据组合，严禁将 MCP JSON 裸文本倾倒给模型。
     */
    class AgentContextBuilder {
    public:
        AgentContextBuilder() = default;
        ~AgentContextBuilder() = default;

        /**
         * @brief 组装并生成标准的 System Prompt
         */
        QString buildSystemPrompt(
            const AgentRunContext& runContext,
            const domain::project::ProjectContext& projectContext,
            const QList<domain::agent::Skill>& activeSkills = {}
        ) const;
    };

} // namespace agent::runtime
