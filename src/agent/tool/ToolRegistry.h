#pragma once

#include <QString>
#include <QList>
#include <QHash>
#include <memory>
#include <mutex>
#include "application/ports/ITool.h"
#include "application/ports/IToolProvider.h"
#include "domain/agent/ToolDefinition.h"
#include "domain/agent/ToolExecution.h"

namespace agent::tool {

    /**
     * @brief 工具注册表（管理内置工具与扩展/MCP工具的注册、查找、元数据汇总与安全调用）
     */
    class ToolRegistry {
    public:
        ToolRegistry() = default;
        ~ToolRegistry() = default;

        /**
         * @brief 注册单个工具实例
         * @return 注册成功返回 true；若工具重名则返回 false
         */
        bool registerTool(std::shared_ptr<application::ports::ITool> tool);

        /**
         * @brief 注册工具提供者并批量载入其提供的工具
         * @return 成功载入的工具数量
         */
        int registerProvider(std::shared_ptr<application::ports::IToolProvider> provider);

        /**
         * @brief 移除指定名称的工具
         */
        void unregisterTool(const QString& name);

        /**
         * @brief 清空所有已注册工具
         */
        void clear();

        /**
         * @brief 查找指定工具实例
         */
        std::shared_ptr<application::ports::ITool> findTool(const QString& name) const;

        /**
         * @brief 是否已注册指定工具
         */
        bool hasTool(const QString& name) const;

        /**
         * @brief 汇总所有已注册工具的 Function Calling 声明列表
         */
        QList<domain::agent::ToolDefinition> definitions() const;

        /**
         * @brief 统一分发并执行工具调用
         * @param call 模型发起的工具调用指令
         * @param context 包含工作区根路径的执行上下文
         * @return 执行结果（未找到工具时返回统一的标准化错误）
         */
        domain::agent::ToolResult execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        );

    private:
        mutable std::mutex m_mutex;
        QHash<QString, std::shared_ptr<application::ports::ITool>> m_tools;
        QList<std::shared_ptr<application::ports::IToolProvider>> m_providers;
    };

} // namespace agent::tool
