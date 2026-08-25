#pragma once

#include <QString>

namespace application::ports {

    /**
     * @brief 工具执行特征属性（供并发调度器、事务恢复与线程策略使用）
     */
    struct ToolExecutionTraits {
        bool threadSafe = false;      ///< 是否允许在 Worker Thread 调用
        bool parallelizable = false;  ///< 是否允许与其他 Tool 同时并发执行
        bool idempotent = false;      ///< 崩溃恢复时是否允许安全自动重放
        QString concurrencyKey;       ///< 串行隔离键名（相同 Key 的工具必须串行排队）

        bool operator==(const ToolExecutionTraits&) const = default;
    };

} // namespace application::ports
