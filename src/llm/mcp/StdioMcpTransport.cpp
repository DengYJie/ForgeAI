#include "StdioMcpTransport.h"

#include <QJsonDocument>
#include <QProcessEnvironment>

namespace llm::mcp {

    StdioMcpTransport::StdioMcpTransport(const McpServerConfig& config, QObject* parent)
        : IMcpTransport(parent), m_config(config) {
    }

    StdioMcpTransport::~StdioMcpTransport() {
        close();
    }

    bool StdioMcpTransport::start() {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            return true;
        }

        close();

        m_process = new QProcess(this);
        if (!m_config.cwd.isEmpty()) {
            m_process->setWorkingDirectory(m_config.cwd);
        }

        if (!m_config.env.isEmpty()) {
            auto env = QProcessEnvironment::systemEnvironment();
            for (auto it = m_config.env.cbegin(); it != m_config.env.cend(); ++it) {
                env.insert(it.key(), it.value());
            }
            m_process->setProcessEnvironment(env);
        }

        connect(m_process, &QProcess::readyReadStandardOutput, this, &StdioMcpTransport::onReadyReadStandardOutput);
        connect(m_process, &QProcess::readyReadStandardError, this, &StdioMcpTransport::onReadyReadStandardError);
        connect(m_process, &QProcess::finished, this, &StdioMcpTransport::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &StdioMcpTransport::onProcessError);

        m_process->start(m_config.command, m_config.args);
        return m_process->waitForStarted(5000);
    }

    void StdioMcpTransport::close() {
        if (m_process) {
            m_process->disconnect(this);
            if (m_process->state() != QProcess::NotRunning) {
                m_process->terminate();
                if (!m_process->waitForFinished(2000)) {
                    m_process->kill();
                    m_process->waitForFinished(1000);
                }
            }
            m_process->deleteLater();
            m_process = nullptr;
            emit closed();
        }
        m_readBuffer.clear();
    }

    bool StdioMcpTransport::sendJson(const QJsonObject& json) {
        if (!isConnected()) return false;

        const QByteArray bytes = QJsonDocument(json).toJson(QJsonDocument::Compact) + "\n";
        qint64 written = m_process->write(bytes);
        return written == bytes.size();
    }

    bool StdioMcpTransport::isConnected() const {
        return m_process != nullptr && m_process->state() == QProcess::Running;
    }

    void StdioMcpTransport::onReadyReadStandardOutput() {
        if (!m_process) return;

        m_readBuffer.append(m_process->readAllStandardOutput());

        while (true) {
            int newlineIndex = m_readBuffer.indexOf('\n');
            if (newlineIndex == -1) break;

            QByteArray line = m_readBuffer.left(newlineIndex).trimmed();
            m_readBuffer.remove(0, newlineIndex + 1);

            if (line.isEmpty()) continue;

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                emit messageReceived(doc.object());
            } else {
                emit errorOccurred(QStringLiteral("JSON 解析错误: %1. 原始行: %2")
                                   .arg(parseError.errorString(), QString::fromUtf8(line)));
            }
        }
    }

    void StdioMcpTransport::onReadyReadStandardError() {
        if (!m_process) return;
        const QString errStr = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (!errStr.isEmpty()) {
            emit errorOccurred(errStr);
        }
    }

    void StdioMcpTransport::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitCode);
        Q_UNUSED(exitStatus);
        emit closed();
    }

    void StdioMcpTransport::onProcessError(QProcess::ProcessError error) {
        QString errStr;
        switch (error) {
        case QProcess::FailedToStart:
            errStr = QStringLiteral("MCP 进程启动失败，未找到可执行程序或权限不足: %1").arg(m_config.command);
            break;
        case QProcess::Crashed:
            errStr = QStringLiteral("MCP 进程意外崩溃: %1").arg(m_config.command);
            break;
        case QProcess::Timedout:
            errStr = QStringLiteral("MCP 操作超时");
            break;
        default:
            errStr = QStringLiteral("MCP 进程未知错误: %1").arg(m_process ? m_process->errorString() : QString());
            break;
        }
        emit errorOccurred(errStr);
    }

} // namespace llm::mcp
