#pragma once
#include "ILogger.h"
#include "sinks/ILogSink.h"
#include <memory>
#include <vector>
#include <deque>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

namespace core::logging {

    /**
     * @brief 异步日志服务
     */
    class LoggingService : public ILogger {
    public:
        static LoggingService &instance();

        LoggingService();
        ~LoggingService() override;

        void addSink(std::shared_ptr<ILogSink> sink);
        void setMinLevel(LogLevel level);
        LogLevel minLevel() const override;

        void setMaxQueueSize(size_t maxSize);
        size_t maxQueueSize() const;

        void log(
            LogLevel level,
            const QString &category,
            const QString &message,
            const QMap<QString, QString> &fields = {},
            std::shared_ptr<const LogContext> context = nullptr) override;

        void dispatch(LogRecord record);
        void flush();

        /**
         * @brief 显式停止后台工作线程并 flush 所有 sink。
         *
         * 必须在 QApplication 析构前调用，保证工作线程完全退出。
         * 析构函数会自动调用本方法，但显式提前调用可控制顺序。
         */
        void shutdown();

        void installQtMessageHandler();

    private:
        void startWorkerThread();
        void stopWorkerThread();
        void workerLoop();

        std::vector<std::shared_ptr<ILogSink>> m_sinks;
        std::atomic<LogLevel> m_minLevel{LogLevel::Debug};
        std::atomic<size_t> m_maxQueueSize{8192};

        std::atomic<uint64_t> m_droppedDebugCount{0};
        std::atomic<uint64_t> m_droppedInfoCount{0};

        std::deque<LogRecord> m_queue;
        QMutex m_queueMutex;
        QWaitCondition m_condition;
        std::atomic<bool> m_running{false};
        std::unique_ptr<QThread> m_workerThread;
    };

} // namespace core::logging
