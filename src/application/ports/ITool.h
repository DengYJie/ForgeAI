#pragma once

#include <QString>
#include <QUuid>
#include <QList>
#include <memory>
#include "domain/agent/ToolDefinition.h"
#include "domain/agent/ToolExecution.h"
#include "domain/agent/ToolPermission.h"

namespace application::ports {

    /**
     * @brief 工具执行上下文（包含当前会话、项目与工作区根目录等运行期元数据）
     */
    struct ToolExecutionContext {
        QString workspaceRoot;
        QString conversationId;
        QUuid projectId;
    };

    /**
     * @brief 工具抽象接口
     */
    class ITool {
    public:
        virtual ~ITool() = default;

        /**
         * @brief 获取该工具的 JSON Schema 定义
         */
        virtual domain::agent::ToolDefinition definition() const = 0;

        /**
         * @brief 获取该工具声明所需的权限要求
         */
        virtual QList<domain::agent::ToolPermission> permissions() const {
            return {};
        }

        /**
         * @brief 执行工具调用
         * @param call 模型下发的工具调用指令（含 id, name, arguments）
         * @param context 运行期执行上下文
         * @return 执行产出的结果实体
         */
        virtual domain::agent::ToolResult execute(
            const domain::agent::ToolCall& call,
            const ToolExecutionContext& context
        ) = 0;
    };

} // namespace application::ports
