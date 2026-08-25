#pragma once

#include <QString>
#include <QUuid>
#include <optional>
#include <functional>
#include "domain/agent/task/ProcessTaskSpec.h"
#include "domain/agent/task/ProcessTaskSnapshot.h"
#include "domain/agent/task/ProcessOutputDelta.h"
#include "ITool.h"

namespace application::ports {

    /**
     * @brief 进程任务运行时抽象端口接口
     * @details 负责操作系统进程池的全异步生命周期管理、增量游标缓冲与安全隔离。
     */
    class IProcessTaskRuntime {
    public:
        virtual ~IProcessTaskRuntime() = default;

        /**
         * @brief 启动一个进程任务并纳入运行时管理
         * @return 生成的唯一任务 ID (格式: task_<uuid>)
         */
        virtual QString start(
            const domain::agent::task::ProcessTaskSpec& spec,
            const ToolExecutionContext& context
        ) = 0;

        /**
         * @brief 获取指定任务的实时只读快照
         */
        virtual std::optional<domain::agent::task::ProcessTaskSnapshot> snapshot(
            const QString& taskId
        ) const = 0;

        /**
         * @brief 从指定游标位置读取增量输出
         * @param taskId 任务 ID
         * @param stdoutCursor 当前已消费的 stdout 绝对字节游标
         * @param stderrCursor 当前已消费的 stderr 绝对字节游标
         * @param maxOutputBytes 单次增量读取最大字节数
         * @param callerRunId 调用方 RunId（用于跨 Session 属主安全校验）
         */
        virtual domain::agent::task::ProcessOutputDelta readDelta(
            const QString& taskId,
            quint64 stdoutCursor = 0,
            quint64 stderrCursor = 0,
            int maxOutputBytes = 32768,
            const QUuid& callerRunId = {}
        ) = 0;

        /**
         * @brief 主动取消或终止指定任务
         */
        virtual bool cancel(
            const QString& taskId,
            const QUuid& callerRunId = {}
        ) = 0;

        /**
         * @brief 取消指定 Agent Run 会话派生的所有后台任务
         */
        virtual void cancelTasksForRun(const QUuid& runId) = 0;

        /**
         * @brief 取消指定工作区项目下的所有后台任务（用于项目切换与卸载）
         */
        virtual void cancelTasksForProject(const QUuid& projectId) = 0;

        /**
         * @brief 应用程序关闭时的优雅资源回收
         */
        virtual void shutdown() = 0;

        /**
         * @brief 异步等待任务产生新输出、状态变更或超时（非阻塞长轮询）
         * @param taskId 目标任务 ID
         * @param stdoutCursor 关注的 stdout 游标
         * @param stderrCursor 关注的 stderr 游标
         * @param waitMs 最大等待毫秒数（0~5000ms）
         * @param callback 唤醒回调（在主线程事件循环中触发）
         */
        virtual void waitForUpdateAsync(
            const QString& taskId,
            quint64 stdoutCursor,
            quint64 stderrCursor,
            int waitMs,
            std::function<void()> callback
        ) = 0;
    };

} // namespace application::ports
