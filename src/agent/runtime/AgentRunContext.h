#pragma once

#include <QUuid>
#include <QString>
#include "domain/model/ResolvedModel.h"
#include "domain/agent/AgentPolicy.h"

namespace agent::runtime {

    /**
     * @brief 工具启用选择模式
     */
    enum class ToolSelectionMode {
        None,      ///< 禁用任何工具
        Selected,  ///< 仅启用 enabledTools 列表中指定的工具
        All        ///< 启用注册表中的所有可用工具
    };

    /**
     * @brief Agent 单次运行的上下文配置参数
     */
    struct AgentRunContext {
        QUuid runId;
        QString sessionId;
        QUuid projectId;
        QString workspaceRoot;

        QString systemPrompt;
        domain::model::ResolvedModel model = []() {
            domain::model::ResolvedModel m;
            domain::model::CanonicalModel c;
            c.capabilities = domain::model::ModelCapability::Chat |
                             domain::model::ModelCapability::Streaming |
                             domain::model::ModelCapability::ToolCalling;
            c.limits.maxOutput = 8192;
            m.canonical = c;
            return m;
        }(); ///< 统一聚合模型实体

        bool useWebSearch = false;
        bool useDeepThinking = false;
        QString reasoningEffort;

        ToolSelectionMode toolSelectionMode = ToolSelectionMode::All;
        QStringList enabledTools;

        domain::agent::AgentPolicy policy;
    };

} // namespace agent::runtime
