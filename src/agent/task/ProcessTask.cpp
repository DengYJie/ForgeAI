#include "ProcessTask.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QDateTime>

namespace agent::task {

    ProcessTask::ProcessTask(
        QString taskId,
        domain::agent::task::ProcessTaskSpec spec,
        QObject* parent
    ) : QObject(parent),
        m_taskId(std::move(taskId)),
        m_spec(std::move(spec)) {
    }

    ProcessTask::~ProcessTask() {
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

    QString ProcessTask::taskId() const {
        return m_taskId;
    }

    domain::agent::task::ProcessTaskState ProcessTask::state() const {
        return m_state;
    }

    const domain::agent::task::ProcessTaskSpec& ProcessTask::spec() const {
        return m_spec;
    }

    void ProcessTask::setState(domain::agent::task::ProcessTaskState newState) {
        if (m_state != newState) {
            m_state = newState;
            emit stateChanged(m_taskId, m_state);
        }
    }

    bool ProcessTask::start() {
        if (m_state != domain::agent::task::ProcessTaskState::Starting) {
            return false;
        }

        m_startedAtMs = QDateTime::currentMSecsSinceEpoch();
        m_elapsed.start();

        m_process = new QProcess(this);
        m_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        if (!m_spec.workingDirectory.isEmpty()) {
            m_process->setWorkingDirectory(m_spec.workingDirectory);
        }

        connect(m_process, &QProcess::readyReadStandardOutput, this, &ProcessTask::onReadyReadStdout);
        connect(m_process, &QProcess::readyReadStandardError, this, &ProcessTask::onReadyReadStderr);
        connect(m_process, &QProcess::finished, this, &ProcessTask::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &ProcessTask::onProcessError);
        connect(m_process, &QProcess::started, this, [this]() {
            m_pid = m_process->processId();
            m_process->closeWriteChannel();
            if (m_state == domain::agent::task::ProcessTaskState::Starting) {
                setState(domain::agent::task::ProcessTaskState::Running);
            }
        });

        if (m_spec.timeoutMs > 0) {
            m_timeoutTimer = new QTimer(this);
            m_timeoutTimer->setSingleShot(true);
            connect(m_timeoutTimer, &QTimer::timeout, this, &ProcessTask::onTimeout);
            m_timeoutTimer->start(m_spec.timeoutMs);
        }

        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("启动进程任务"), {
            {QStringLiteral("taskId"), m_taskId},
            {QStringLiteral("program"), m_spec.program},
            {QStringLiteral("workingDir"), m_spec.workingDirectory},
            {QStringLiteral("timeoutMs"), QString::number(m_spec.timeoutMs)},
            {QStringLiteral("background"), m_spec.background ? QStringLiteral("true") : QStringLiteral("false")}
        });

        m_process->start(m_spec.program, m_spec.arguments);

        return true;
    }

    void ProcessTask::cancel() {
        if (m_state == domain::agent::task::ProcessTaskState::Starting ||
            m_state == domain::agent::task::ProcessTaskState::Running) {

            if (m_timeoutTimer) {
                m_timeoutTimer->stop();
            }

            if (m_process && m_process->state() != QProcess::NotRunning) {
                m_process->kill();
            }

            finalizeTask(domain::agent::task::ProcessTaskState::Cancelled, -1, QStringLiteral("任务已被主动取消"));
        }
    }

    void ProcessTask::onReadyReadStdout() {
        if (!m_process) return;
        const QByteArray data = m_process->readAllStandardOutput();
        if (!data.isEmpty()) {
            m_stdoutBuffer.append(data);
            emit outputAppended(m_taskId);
        }
    }

    void ProcessTask::onReadyReadStderr() {
        if (!m_process) return;
        const QByteArray data = m_process->readAllStandardError();
        if (!data.isEmpty()) {
            m_stderrBuffer.append(data);
            emit outputAppended(m_taskId);
        }
    }

    void ProcessTask::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        if (m_finishedEmitted) return;
        if (m_timeoutTimer) m_timeoutTimer->stop();

        onReadyReadStdout();
        onReadyReadStderr();

        m_exitCode = exitCode;

        if (exitStatus == QProcess::CrashExit && !m_timedOut && m_state != domain::agent::task::ProcessTaskState::Cancelled) {
            finalizeTask(domain::agent::task::ProcessTaskState::Crashed, exitCode, QStringLiteral("进程崩溃异常退出"));
        } else if (exitCode == 0) {
            finalizeTask(domain::agent::task::ProcessTaskState::Completed, exitCode);
        } else {
            finalizeTask(domain::agent::task::ProcessTaskState::Failed, exitCode, QStringLiteral("进程退出码为 %1").arg(exitCode));
        }
    }

    void ProcessTask::onProcessError(QProcess::ProcessError error) {
        if (m_finishedEmitted) return;
        if (m_state == domain::agent::task::ProcessTaskState::Cancelled) return;

        if (error == QProcess::FailedToStart) {
            if (m_timeoutTimer) m_timeoutTimer->stop();
            m_exitCode = -1;
            finalizeTask(domain::agent::task::ProcessTaskState::Failed, -1, QStringLiteral("无法启动程序 '%1'，未找到可执行文件或缺少权限").arg(m_spec.program));
        }
    }

    void ProcessTask::onTimeout() {
        if (m_finishedEmitted) return;
        m_timedOut = true;

        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }

        m_exitCode = -1;
        finalizeTask(domain::agent::task::ProcessTaskState::TimedOut, -1, QStringLiteral("执行超时 (超过 %1 ms)").arg(m_spec.timeoutMs));
    }

    void ProcessTask::finalizeTask(
        domain::agent::task::ProcessTaskState finalState,
        int exitCode,
        const QString& exitError
    ) {
        if (m_finishedEmitted) return;
        m_finishedEmitted = true;

        m_finishedAtMs = QDateTime::currentMSecsSinceEpoch();
        m_exitCode = exitCode;
        m_exitError = exitError;

        setState(finalState);

        core::logging::LoggingService::instance().info(core::logging::Category::AgentRuntime, QStringLiteral("进程任务结束"), {
            {QStringLiteral("taskId"), m_taskId},
            {QStringLiteral("state"), QString::number(static_cast<int>(finalState))},
            {QStringLiteral("exitCode"), QString::number(exitCode)},
            {QStringLiteral("durationMs"), QString::number(m_elapsed.elapsed())}
        });

        emit finished(m_taskId, finalState, exitCode);
    }

    domain::agent::task::ProcessTaskSnapshot ProcessTask::snapshot() const {
        domain::agent::task::ProcessTaskSnapshot snap;
        snap.taskId = m_taskId;
        snap.state = m_state;
        snap.program = m_spec.program;
        snap.arguments = m_spec.arguments;
        snap.workingDirectory = m_spec.workingDirectory;
        snap.pid = m_pid;
        snap.exitCode = m_exitCode;
        snap.startedAtMs = m_startedAtMs;
        snap.finishedAtMs = m_finishedAtMs;
        snap.stdoutTotalBytes = m_stdoutBuffer.totalProducedBytes();
        snap.stderrTotalBytes = m_stderrBuffer.totalProducedBytes();
        snap.stdoutTruncated = m_stdoutBuffer.hasTruncated();
        snap.stderrTruncated = m_stderrBuffer.hasTruncated();
        snap.exitError = m_exitError;
        snap.runId = m_spec.runId;
        snap.projectId = m_spec.projectId;
        snap.workspaceRoot = m_spec.workspaceRoot;
        return snap;
    }

    domain::agent::task::ProcessOutputDelta ProcessTask::readDelta(
        quint64 stdoutCursor,
        quint64 stderrCursor,
        int maxOutputBytes
    ) const {
        domain::agent::task::ProcessOutputDelta delta;
        delta.taskId = m_taskId;
        delta.state = m_state;
        delta.finished = (m_state == domain::agent::task::ProcessTaskState::Completed ||
                          m_state == domain::agent::task::ProcessTaskState::Failed ||
                          m_state == domain::agent::task::ProcessTaskState::TimedOut ||
                          m_state == domain::agent::task::ProcessTaskState::Cancelled ||
                          m_state == domain::agent::task::ProcessTaskState::Crashed);
        delta.exitCode = m_exitCode;
        delta.exitError = m_exitError;
        delta.durationMs = m_elapsed.isValid() ? m_elapsed.elapsed() : 0;

        delta.stdoutDelta = m_stdoutBuffer.readFrom(
            stdoutCursor,
            maxOutputBytes,
            &delta.stdoutCursorLost,
            &delta.stdoutAvailableFrom,
            &delta.nextStdoutCursor
        );

        delta.stderrDelta = m_stderrBuffer.readFrom(
            stderrCursor,
            maxOutputBytes,
            &delta.stderrCursorLost,
            &delta.stderrAvailableFrom,
            &delta.nextStderrCursor
        );

        return delta;
    }

} // namespace agent::task
