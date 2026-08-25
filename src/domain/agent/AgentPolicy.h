#pragma once

#include "ToolPermission.h"
#include <QSet>
#include <QMap>
#include <QString>

namespace domain::agent {

    /**
     * @brief Agent 运行策略与安全约束
     */
    struct AgentPolicy {
        int maxToolRounds = 8;                  ///< 最大允许的连续工具调用轮数
        int timeoutMs = 120000;                 ///< 单轮模型调用超时时间 (ms)
        int toolTimeoutMs = 0;                  ///< 单个工具执行超时时间 (ms，0 表示继承 timeoutMs 或默认 30000ms)
        int maxToolOutputChars = 32768;         ///< 单个工具最大输出字符截断保护阈值 (chars)
        bool allowParallelToolExecution = true; ///< 是否允许同轮内的并发工具执行

        bool autoApproveReadOnly = true;        ///< 默认自动授权只读操作
        bool autoApproveWriteWorkspace = true;  ///< 工作区写操作策略
        bool autoApproveExecuteProcess = false; ///< 进程执行默认需确认
        bool autoApproveNetwork = true;         ///< 网络访问策略
        bool autoApproveExternalService = true; ///< 外部服务/MCP 策略
        bool autoApproveDestructive = false;    ///< 破坏性操作默认绝对需确认

        /**
         * @brief 工具/服务级权限覆盖规则表
         * @details 支持以下层级匹配：
         *          1. 精确工具名: "write_file", "mcp_db.query_sql"
         *          2. 服务通配符: "mcp_db.*"
         *          3. 全局通配符: "*"
         */
        QMap<QString, PermissionDecision> toolRules;

        /**
         * @brief 根据权限类型判定默认决策
         */
        PermissionDecision evaluatePermission(ToolPermissionType type) const {
            switch (type) {
            case ToolPermissionType::ReadOnly:
            case ToolPermissionType::FileSystemRead:
            case ToolPermissionType::NetworkRead:
            case ToolPermissionType::ExternalServiceRead:
                return autoApproveReadOnly ? PermissionDecision::Allow : PermissionDecision::AskUser;

            case ToolPermissionType::WriteWorkspace:
            case ToolPermissionType::FileSystemWrite:
                return autoApproveWriteWorkspace ? PermissionDecision::Allow : PermissionDecision::AskUser;

            case ToolPermissionType::ExecuteProcess:
            case ToolPermissionType::ProcessExecute:
                return autoApproveExecuteProcess ? PermissionDecision::Allow : PermissionDecision::AskUser;

            case ToolPermissionType::Network:
            case ToolPermissionType::NetworkWrite:
                return autoApproveNetwork ? PermissionDecision::Allow : PermissionDecision::AskUser;

            case ToolPermissionType::ExternalService:
            case ToolPermissionType::ExternalServiceWrite:
                return autoApproveExternalService ? PermissionDecision::Allow : PermissionDecision::AskUser;

            case ToolPermissionType::DestructiveOperation:
                return autoApproveDestructive ? PermissionDecision::Allow : PermissionDecision::AskUser;
            }
            return PermissionDecision::Allow;
        }

        /**
         * @brief 综合工具名通配符规则与权限类型进行层级裁决
         * @details 裁决优先级: 精确工具规则 > 服务通配符规则 (server.*) > 全局通配符规则 (*) > 权限类型默认值
         */
        PermissionDecision evaluateTool(const QString& toolName, const ToolPermission& perm) const {
            // 1. 精确工具名匹配
            if (toolRules.contains(toolName)) {
                return toolRules.value(toolName);
            }

            // 2. 服务级通配符匹配 (如 "server_name.tool_func" -> "server_name.*")
            const int dotIdx = toolName.indexOf(QLatin1Char('.'));
            if (dotIdx > 0) {
                const QString serverWildcard = toolName.left(dotIdx) + QStringLiteral(".*");
                if (toolRules.contains(serverWildcard)) {
                    return toolRules.value(serverWildcard);
                }
            }
            const int colonIdx = toolName.indexOf(QStringLiteral("::"));
            if (colonIdx > 0) {
                const QString serverWildcard = toolName.left(colonIdx) + QStringLiteral("::*");
                if (toolRules.contains(serverWildcard)) {
                    return toolRules.value(serverWildcard);
                }
            }

            // 3. 全局通配符匹配
            if (toolRules.contains(QStringLiteral("*"))) {
                return toolRules.value(QStringLiteral("*"));
            }

            // 4. 回退至权限类型规则
            return evaluatePermission(perm.type);
        }

        bool operator==(const AgentPolicy &other) const = default;
    };

} // namespace domain::agent
