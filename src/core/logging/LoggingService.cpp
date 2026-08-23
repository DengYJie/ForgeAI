#include "LoggingService.h"
#include <QtGlobal>
#include <QDateTime>

namespace core::logging {

    static void customQtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        LogLevel level = LogLevel::Info;
        switch (type) {
            case QtDebugMsg:    level = LogLevel::Debug; break;
            case QtInfoMsg:     level = LogLevel::Info; break;
            case QtWarningMsg:  level = LogLevel::Warning; break;
            case QtCriticalMsg: level = LogLevel::Error; break;
            case QtFatalMsg:    level = LogLevel::Critical; break;
        }

        QString category = (context.category && qstrcmp(context.category, "default") != 0)
                               ? QString::fromUtf8(context.category)
                               : QStringLiteral("qt");

        LoggingService::instance().log(level, category, msg);
    }

    LoggingService &LoggingService::instance() {
        static LoggingService s_instance;
        return s_instance;
    }

    LoggingService::LoggingService() {
        startWorkerThread();
    }

    LoggingService::~LoggingService() {
        shutdown();
    }

    void LoggingService::addSink(std::shared_ptr<ILogSink> sink) {
        QMutexLocker locker(&m_queueMutex);
        if (sink) {
            m_sinks.push_back(std::move(sink));
        }
    }

    void LoggingService::setMinLevel(LogLevel level) {
        m_minLevel = level;
    }

    LogLevel LoggingService::minLevel() const {
        return m_minLevel.load();
    }

    void LoggingService::setMaxQueueSize(size_t maxSize) {
        m_maxQueueSize = maxSize > 128 ? maxSize : 128;
    }

    size_t LoggingService::maxQueueSize() const {
        return m_maxQueueSize.load();
    }

    void LoggingService::installQtMessageHandler() {
        qInstallMessageHandler(customQtMessageHandler);
    }

    void LoggingService::shutdown() {
        // 还原 Qt 消息处理器，避免 QApplication 析构后再触发日志调用
        qInstallMessageHandler(nullptr);
        stopWorkerThread();
    }

    void LoggingService::log(
        LogLevel level,
        const QString &category,
        const QString &message,
        const QMap<QString, QString> &fields,
        std::shared_ptr<const LogContext> context) {
        
        if (!isEnabled(level)) {
            return;
        }

        LogRecord record(level, category, message, fields, std::move(context));
        dispatch(std::move(record));
    }

    void LoggingService::dispatch(LogRecord record) {
        QMutexLocker locker(&m_queueMutex);
        size_t limit = m_maxQueueSize.load();

        if (m_queue.size() >= limit) {
            if (record.level == LogLevel::Trace || record.level == LogLevel::Debug) {
                m_droppedDebugCount.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            if (record.level == LogLevel::Info) {
                m_droppedInfoCount.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            if (m_queue.size() >= limit * 3 / 2) {
                while (!m_queue.empty() && m_queue.front().level <= LogLevel::Info) {
                    m_queue.pop_front();
                    m_droppedInfoCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        m_queue.push_back(std::move(record));
        m_condition.wakeOne();
    }

    void LoggingService::flush() {
        // 等待队列清空
        while (true) {
            {
                QMutexLocker locker(&m_queueMutex);
                if (m_queue.empty()) {
                    break;
                }
            }
            QThread::msleep(10);
        }

        QMutexLocker locker(&m_queueMutex);
        for (auto &sink : m_sinks) {
            if (sink) sink->flush();
        }
    }

    void LoggingService::startWorkerThread() {
        m_running = true;
        m_workerThread = std::unique_ptr<QThread>(QThread::create([this]() {
            workerLoop();
        }));
        m_workerThread->start();
    }

    void LoggingService::stopWorkerThread() {
        if (!m_running) return;

        m_running = false;
        m_condition.wakeAll();

        if (m_workerThread && m_workerThread->isRunning()) {
            m_workerThread->wait(2000);
        }

        // 处理剩余的高优先级日志记录
        std::vector<LogRecord> remaining;
        {
            QMutexLocker locker(&m_queueMutex);
            remaining.reserve(m_queue.size());
            while (!m_queue.empty()) {
                remaining.push_back(std::move(m_queue.front()));
                m_queue.pop_front();
            }
        }

        for (auto &sink : m_sinks) {
            if (sink) {
                sink->writeBatch(remaining);
                sink->flush();
            }
        }
    }

    void LoggingService::workerLoop() {
        qint64 lastFlushTime = QDateTime::currentMSecsSinceEpoch();

        while (m_running) {
            std::vector<LogRecord> batch;
            batch.reserve(256);
            bool hasHighSeverity = false;

            {
                QMutexLocker locker(&m_queueMutex);
                while (m_queue.empty() && m_running) {
                    // 超时等待 1000ms 触发周期性 Flush
                    bool signaled = m_condition.wait(&m_queueMutex, 1000);
                    if (!signaled) {
                        break; // 超时，去检查 periodic flush
                    }
                }

                if (!m_running && m_queue.empty()) {
                    break;
                }

                // 检查是否有过载丢弃统计需生成报告
                uint64_t droppedDebug = m_droppedDebugCount.exchange(0, std::memory_order_relaxed);
                uint64_t droppedInfo = m_droppedInfoCount.exchange(0, std::memory_order_relaxed);
                if (droppedDebug > 0 || droppedInfo > 0) {
                    LogRecord overloadReport;
                    overloadReport.level = LogLevel::Warning;
                    overloadReport.category = QStringLiteral("app.logging");
                    overloadReport.message = QStringLiteral("Dropped %1 debug/trace and %2 info log records due to queue overload")
                                                .arg(droppedDebug).arg(droppedInfo);
                    batch.push_back(std::move(overloadReport));
                }

                // 批量提取最多 256 条日志
                while (!m_queue.empty() && batch.size() < 256) {
                    auto rec = std::move(m_queue.front());
                    m_queue.pop_front();
                    if (rec.level >= LogLevel::Error) {
                        hasHighSeverity = true;
                    }
                    batch.push_back(std::move(rec));
                }
            }

            if (!batch.empty()) {
                for (auto &sink : m_sinks) {
                    if (sink) {
                        sink->writeBatch(batch);
                    }
                }
            }

            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (hasHighSeverity || (now - lastFlushTime >= 1000)) {
                for (auto &sink : m_sinks) {
                    if (sink) sink->flush();
                }
                lastFlushTime = now;
            }
        }
    }

} // namespace core::logging
