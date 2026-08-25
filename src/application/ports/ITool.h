#pragma once

#include <QString>
#include <QUuid>
#include <QList>
#include <memory>
#include <atomic>
#include "domain/agent/ToolDefinition.h"
#include "domain/agent/ToolExecution.h"
#include "domain/agent/ToolPermission.h"
#include "ToolExecutionTraits.h"
#include "IToolOperation.h"

namespace application::ports {

    /**
     * @brief 线程安全的取消令牌（支持合作式终止慢速或挂起任务）
     */
    class CancellationToken {
    public:
        CancellationToken() : m_canceled(std::make_shared<std::atomic<bool>>(false)) {}

        bool isCanceled() const {
            if (m_canceled && m_canceled->load(std::memory_order_relaxed)) {
                return true;
            }
            if (m_parent && m_parent->isCanceled()) {
                return true;
            }
            return false;
        }

        void cancel() {
            if (m_canceled) {
                m_canceled->store(true, std::memory_order_relaxed);
            }
        }

        void linkParent(const CancellationToken& parent) {
            m_parent = std::make_shared<CancellationToken>(parent);
        }

    private:
        std::shared_ptr<std::atomic<bool>> m_canceled;
        std::shared_ptr<CancellationToken> m_parent;
    };

    /**
     * @brief 工具执行上下文（包含当前会话、项目与工作区根目录等运行期元数据）
     */
    struct ToolExecutionContext {
        QUuid runId;
        QString conversationId;
        QUuid projectId;
        QString workspaceRoot;
        int timeoutMs = 30000;
        CancellationToken cancellationToken;
        QString executionId;
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
         * @brief 获取工具执行特征属性
         */
        virtual ToolExecutionTraits traits() const = 0;

        /**
         * @brief 根据具体调用参数动态获取所需权限要求
         */
        virtual QList<domain::agent::ToolPermission> permissions(const domain::agent::ToolCall& call) const {
            Q_UNUSED(call);
            return permissions();
        }

        /**
         * @brief 获取该工具声明所需的默认静态权限要求
         */
        virtual QList<domain::agent::ToolPermission> permissions() const {
            return {};
        }

        /**
         * @brief 异步执行工具调用
         * @param call 模型下发的工具调用指令（含 id, name, arguments）
         * @param context 运行期执行上下文
         * @return 异步操作实例（生命周期由调用方管理）
         */
        virtual std::unique_ptr<IToolOperation> execute(
            const domain::agent::ToolCall& call,
            const ToolExecutionContext& context
        ) = 0;
    };

} // namespace application::ports
