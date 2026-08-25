#include "ProcessToolOperation.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace agent::tool::builtin {

    namespace {
        constexpr int MAX_STREAM_BYTES = 256 * 1024; // 256 KB
        constexpr int HEAD_TAIL_BYTES = 128 * 1024;  // 128 KB
    }

    ProcessToolOperation::ProcessToolOperation(
        const QString& operationId,
        QString program,
        QStringList args,
        QString workingDirectory,
        int timeoutMs,
        QObject* parent
    ) : application::ports::IToolOperation(parent),
        m_operationId(operationId),
        m_program(std::move(program)),
        m_args(std::move(args)),
        m_workingDirectory(std::move(workingDirectory)),
        m_timeoutMs(timeoutMs) {}

    ProcessToolOperation::~ProcessToolOperation() {
        if (m_timeoutTimer) {
            m_timeoutTimer->stop();
        }
        if (m_process) {
            if (m_process->state() != QProcess::NotRunning) {
                m_process->kill();
                m_process->waitForFinished(100);
            }
        }
    }

    QString ProcessToolOperation::operationId() const {
        return m_operationId;
    }

    application::ports::ToolOperationState ProcessToolOperation::state() const {
        return m_state;
    }

    void ProcessToolOperation::start() {
        if (m_state != application::ports::ToolOperationState::Created) return;
        m_state = application::ports::ToolOperationState::Running;
        m_elapsed.start();

        m_process = new QProcess(this);
        if (!m_workingDirectory.isEmpty()) {
            m_process->setWorkingDirectory(m_workingDirectory);
        }

        connect(m_process, &QProcess::readyReadStandardOutput, this, &ProcessToolOperation::onReadyReadStdout);
        connect(m_process, &QProcess::readyReadStandardError, this, &ProcessToolOperation::onReadyReadStderr);
        connect(m_process, &QProcess::finished, this, &ProcessToolOperation::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &ProcessToolOperation::onProcessError);

        if (m_timeoutMs > 0) {
            m_timeoutTimer = new QTimer(this);
            m_timeoutTimer->setSingleShot(true);
            connect(m_timeoutTimer, &QTimer::timeout, this, &ProcessToolOperation::onTimeout);
            m_timeoutTimer->start(m_timeoutMs);
        }

        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("启动子进程"), {
            {QStringLiteral("program"), m_program},
            {QStringLiteral("workingDir"), m_workingDirectory},
            {QStringLiteral("timeoutMs"), QString::number(m_timeoutMs)}
        });

        m_process->start(m_program, m_args);
    }

    void ProcessToolOperation::cancel() {
        if (m_state == application::ports::ToolOperationState::Created || m_state == application::ports::ToolOperationState::Running) {
            m_state = application::ports::ToolOperationState::Cancelled;
            if (m_timeoutTimer) m_timeoutTimer->stop();

            if (m_process && m_process->state() != QProcess::NotRunning) {
                m_process->kill();
            }

            if (!m_finishedEmitted) {
                m_finishedEmitted = true;
                emit finished(domain::agent::ToolResult{
                    m_operationId,
                    QStringLiteral("命令执行已由用户取消"),
                    true,
                    QStringLiteral("Cancelled"),
                    {}
                });
            }
        }
    }

    void ProcessToolOperation::onReadyReadStdout() {
        if (!m_process) return;
        const QByteArray chunk = m_process->readAllStandardOutput();
        m_totalStdoutBytes += chunk.size();
        if (m_stdoutBuffer.size() < MAX_STREAM_BYTES * 2) {
            m_stdoutBuffer.append(chunk);
        }
    }

    void ProcessToolOperation::onReadyReadStderr() {
        if (!m_process) return;
        const QByteArray chunk = m_process->readAllStandardError();
        m_totalStderrBytes += chunk.size();
        if (m_stderrBuffer.size() < MAX_STREAM_BYTES * 2) {
            m_stderrBuffer.append(chunk);
        }
    }

    void ProcessToolOperation::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        if (m_finishedEmitted) return;
        if (m_timeoutTimer) m_timeoutTimer->stop();

        onReadyReadStdout();
        onReadyReadStderr();

        if (exitStatus == QProcess::CrashExit && !m_timedOut && m_state != application::ports::ToolOperationState::Cancelled) {
            finalizeOperation(exitCode, false, QStringLiteral("程序崩溃异常终止"));
        } else {
            finalizeOperation(exitCode, false);
        }
    }

    void ProcessToolOperation::onProcessError(QProcess::ProcessError error) {
        if (m_finishedEmitted) return;
        if (m_state == application::ports::ToolOperationState::Cancelled) return;

        if (error == QProcess::FailedToStart) {
            if (m_timeoutTimer) m_timeoutTimer->stop();
            m_state = application::ports::ToolOperationState::Failed;
            m_finishedEmitted = true;

            const QString errMsg = QStringLiteral("无法启动程序 '%1'，请检查命令名称是否正确或是否已安装并加入 PATH。").arg(m_program);
            QJsonObject meta;
            meta[QStringLiteral("program")] = m_program;
            meta[QStringLiteral("error")] = QStringLiteral("FailedToStart");

            emit finished(domain::agent::ToolResult{
                m_operationId,
                errMsg,
                true,
                QStringLiteral("FailedToStart"),
                meta
            });
        }
    }

    void ProcessToolOperation::onTimeout() {
        if (m_finishedEmitted) return;
        m_timedOut = true;
        m_state = application::ports::ToolOperationState::TimedOut;

        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }

        finalizeOperation(-1, true, QStringLiteral("命令执行超时 (超过 %1 ms)").arg(m_timeoutMs));
    }

    QString ProcessToolOperation::sanitizeOutput(const QByteArray& raw, int maxBytes, bool* truncated) {
        if (truncated) *truncated = false;
        if (raw.size() <= maxBytes) {
            return QString::fromUtf8(raw);
        }

        if (truncated) *truncated = true;
        const QByteArray head = raw.left(HEAD_TAIL_BYTES);
        const QByteArray tail = raw.right(HEAD_TAIL_BYTES);

        return QString::fromUtf8(head) +
            QStringLiteral("\n\n... [中间输出已截断 (总计 %1 字节，仅保留首尾各 %2 KB)] ...\n\n").arg(raw.size()).arg(HEAD_TAIL_BYTES / 1024) +
            QString::fromUtf8(tail);
    }

    void ProcessToolOperation::finalizeOperation(int exitCode, bool timedOut, const QString& errorString) {
        if (m_finishedEmitted) return;
        m_finishedEmitted = true;

        bool stdoutTruncated = false;
        bool stderrTruncated = false;
        const QString stdoutStr = sanitizeOutput(m_stdoutBuffer, MAX_STREAM_BYTES, &stdoutTruncated);
        const QString stderrStr = sanitizeOutput(m_stderrBuffer, MAX_STREAM_BYTES, &stderrTruncated);

        const qint64 durationMs = m_elapsed.elapsed();
        const bool isError = (exitCode != 0 || timedOut || !errorString.isEmpty());

        m_state = isError ? application::ports::ToolOperationState::Failed : application::ports::ToolOperationState::Completed;

        QJsonObject rootObj;
        rootObj[QStringLiteral("exit_code")] = exitCode;
        rootObj[QStringLiteral("stdout")] = stdoutStr;
        rootObj[QStringLiteral("stderr")] = stderrStr;
        rootObj[QStringLiteral("duration_ms")] = durationMs;
        rootObj[QStringLiteral("timed_out")] = timedOut;
        if (!errorString.isEmpty()) {
            rootObj[QStringLiteral("error_message")] = errorString;
        }

        QJsonObject meta;
        meta[QStringLiteral("exit_code")] = exitCode;
        meta[QStringLiteral("duration_ms")] = durationMs;
        meta[QStringLiteral("stdout_bytes")] = m_totalStdoutBytes;
        meta[QStringLiteral("stderr_bytes")] = m_totalStderrBytes;
        meta[QStringLiteral("stdout_truncated")] = stdoutTruncated;
        meta[QStringLiteral("stderr_truncated")] = stderrTruncated;

        const QString jsonContent = QString::fromUtf8(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));

        domain::agent::ToolResult result{
            m_operationId,
            jsonContent,
            isError,
            isError ? (timedOut ? QStringLiteral("TimedOut") : QStringLiteral("CommandFailed")) : QString(),
            meta
        };

        emit finished(result);
    }

} // namespace agent::tool::builtin
