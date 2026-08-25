#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QCoreApplication>
#include <functional>
#include <memory>
#include <thread>
#include "domain/agent/ToolExecution.h"

namespace application::ports {

    /**
     * @brief 工具操作生命周期状态
     */
    enum class ToolOperationState {
        Created,
        Running,
        Completed,
        Failed,
        Cancelled,
        TimedOut
    };

    /**
     * @brief 异步工具操作抽象基类
     */
    class IToolOperation : public QObject {
        Q_OBJECT
    public:
        using QObject::QObject;
        ~IToolOperation() override = default;

        virtual QString operationId() const = 0;
        virtual ToolOperationState state() const = 0;
        virtual void start() = 0;
        virtual void cancel() = 0;

    Q_SIGNALS:
        void finished(const domain::agent::ToolResult& result);
    };

    /**
     * @brief 同步/快速计算任务的异步适配操作
     */
    class ImmediateToolOperation : public IToolOperation {
        Q_OBJECT
    public:
        ImmediateToolOperation(
            const QString& operationId,
            std::function<domain::agent::ToolResult()> work,
            QObject* parent = nullptr
        ) : IToolOperation(parent),
            m_operationId(operationId),
            m_work(std::move(work)) {
        }

        QString operationId() const override { return m_operationId; }
        ToolOperationState state() const override { return m_state; }

        void start() override {
            if (m_state != ToolOperationState::Created) return;
            m_state = ToolOperationState::Running;

            QTimer::singleShot(0, this, [this]() {
                if (m_state == ToolOperationState::Cancelled) return;
                domain::agent::ToolResult result;
                try {
                    if (m_work) {
                        result = m_work();
                    }
                    m_state = result.isError ? ToolOperationState::Failed : ToolOperationState::Completed;
                } catch (const std::exception& ex) {
                    m_state = ToolOperationState::Failed;
                    result = domain::agent::ToolResult{m_operationId, QString::fromLatin1(ex.what()), true};
                } catch (...) {
                    m_state = ToolOperationState::Failed;
                    result = domain::agent::ToolResult{m_operationId, QStringLiteral("执行发生未知异常"), true};
                }
                if (!m_finishedEmitted) {
                    m_finishedEmitted = true;
                    emit finished(result);
                }
            });
        }

        void cancel() override {
            if (m_state == ToolOperationState::Running || m_state == ToolOperationState::Created) {
                m_state = ToolOperationState::Cancelled;
                if (!m_finishedEmitted) {
                    m_finishedEmitted = true;
                    emit finished(domain::agent::ToolResult{m_operationId, QStringLiteral("操作已取消"), true});
                }
            }
        }

    private:
        QString m_operationId;
        std::function<domain::agent::ToolResult()> m_work;
        ToolOperationState m_state = ToolOperationState::Created;
        bool m_finishedEmitted = false;
    };

    /**
     * @brief 在后台线程执行阻塞工作，并附带主线程超时 watchdog 的工具操作
     * @details 适用于 threadSafe=true 且可能长时间阻塞的工具（内置文件工具、慢速 CPU 操作等）。
     *          工作 lambda 在 std::thread 中执行；超时 QTimer 和 finished 信号均在主线程发射。
     */
    class ThreadedToolOperation : public IToolOperation {
        Q_OBJECT
    public:
        ThreadedToolOperation(
            const QString& operationId,
            std::function<domain::agent::ToolResult()> work,
            int timeoutMs = 0,
            QObject* parent = nullptr
        ) : IToolOperation(parent),
            m_operationId(operationId),
            m_work(std::move(work)),
            m_timeoutMs(timeoutMs) {}

        ~ThreadedToolOperation() override = default;

        QString operationId() const override { return m_operationId; }
        ToolOperationState state() const override { return m_state; }

        void start() override {
            if (m_state != ToolOperationState::Created) return;
            m_state = ToolOperationState::Running;

            if (m_timeoutMs > 0) {
                m_timeoutTimer = new QTimer(this);
                m_timeoutTimer->setSingleShot(true);
                connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
                    if (m_finishedEmitted) return;
                    m_state = ToolOperationState::TimedOut;
                    m_finishedEmitted = true;
                    // Defer the emission so the QTimer callback stack can unwind cleanly
                    // before onToolOperationFinished deletes this op (and its child QTimer).
                    // Emitting directly would cause Qt to destroy the single-shot QTimer
                    // while still executing inside its own timerEvent dispatch — UAF.
                    domain::agent::ToolResult timeoutResult{
                        m_operationId,
                        QStringLiteral("工具执行响应超时，请稍后重试。"),
                        true
                    };
                    QPointer<ThreadedToolOperation> weakSelf(this);
                    QMetaObject::invokeMethod(QCoreApplication::instance(), [weakSelf, timeoutResult]() {
                        if (!weakSelf) return;
                        emit weakSelf->finished(timeoutResult);
                    }, Qt::QueuedConnection);
                });
                m_timeoutTimer->start(m_timeoutMs);
            }

            QPointer<ThreadedToolOperation> weakSelf(this);
            auto opId = m_operationId;
            auto work = m_work;

            std::thread([weakSelf, work = std::move(work), opId]() {
                domain::agent::ToolResult result;
                try {
                    if (work) {
                        result = work();
                    } else {
                        result = {opId, QStringLiteral("工具未提供执行函数"), true};
                    }
                } catch (const std::exception& ex) {
                    result = {opId, QString::fromLatin1(ex.what()), true};
                } catch (...) {
                    result = {opId, QStringLiteral("执行发生未知异常"), true};
                }

                // Post result to main thread via stable QCoreApplication receiver.
                // QPointer check inside lambda is safe: the lambda runs on main thread,
                // same thread where ThreadedToolOperation is destroyed, so no race.
                QMetaObject::invokeMethod(QCoreApplication::instance(), [weakSelf, result]() {
                    if (!weakSelf) return; // operation was already destroyed (e.g. timed out)
                    if (weakSelf->m_timeoutTimer) weakSelf->m_timeoutTimer->stop();
                    if (weakSelf->m_finishedEmitted) return;
                    weakSelf->m_finishedEmitted = true;
                    weakSelf->m_state = result.isError ? ToolOperationState::Failed : ToolOperationState::Completed;
                    emit weakSelf->finished(result);
                }, Qt::QueuedConnection);
            }).detach();
        }

        void cancel() override {
            if (m_state == ToolOperationState::Created || m_state == ToolOperationState::Running) {
                m_state = ToolOperationState::Cancelled;
                if (m_timeoutTimer) m_timeoutTimer->stop();
                if (!m_finishedEmitted) {
                    m_finishedEmitted = true;
                    emit finished(domain::agent::ToolResult{m_operationId, QStringLiteral("操作已取消"), true});
                }
            }
        }

    private:
        QString m_operationId;
        std::function<domain::agent::ToolResult()> m_work;
        int m_timeoutMs;
        ToolOperationState m_state = ToolOperationState::Created;
        QTimer* m_timeoutTimer = nullptr;
        bool m_finishedEmitted = false;
    };

} // namespace application::ports
