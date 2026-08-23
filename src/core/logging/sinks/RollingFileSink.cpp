#include "RollingFileSink.h"
#include "core/logging/LogFormatter.h"
#include <QDir>
#include <QFileInfo>

namespace core::logging {

    RollingFileSink::RollingFileSink(
        const QString &logDir,
        const QString &baseFileName,
        qint64 maxFileSize,
        int maxBackupFiles)
        : m_logDir(logDir.isEmpty() ? (QDir::homePath() + QStringLiteral("/.forgeai/logs")) : logDir)
        , m_baseFileName(baseFileName.isEmpty() ? QStringLiteral("app.log") : baseFileName)
        , m_maxFileSize(maxFileSize > 0 ? maxFileSize : 10 * 1024 * 1024)
        , m_maxBackupFiles(maxBackupFiles > 0 ? maxBackupFiles : 10) {
        
        QDir dir(m_logDir);
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }

        openCurrentFile();
    }

    RollingFileSink::~RollingFileSink() {
        flush();
        if (m_file.isOpen()) {
            m_file.close();
        }
    }

    QString RollingFileSink::currentLogFilePath() const {
        return m_logDir + QStringLiteral("/") + m_baseFileName;
    }

    bool RollingFileSink::openCurrentFile() {
        if (m_file.isOpen()) {
            m_file.close();
        }

        m_file.setFileName(currentLogFilePath());
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            m_currentFileSize = 0;
            return false;
        }

        m_currentFileSize = m_file.size();
        return true;
    }

    void RollingFileSink::rotateIfNeeded() {
        if (!m_file.isOpen() || m_currentFileSize < m_maxFileSize) {
            return;
        }

        m_file.flush();
        m_file.close();

        // 滚动历史文件: app.9.log -> app.10.log, ..., app.log -> app.1.log
        QDir dir(m_logDir);
        QString oldestFile = QStringLiteral("%1/%2.%3").arg(m_logDir, m_baseFileName).arg(m_maxBackupFiles);
        if (QFile::exists(oldestFile)) {
            QFile::remove(oldestFile);
        }

        for (int i = m_maxBackupFiles - 1; i >= 1; --i) {
            QString src = QStringLiteral("%1/%2.%3").arg(m_logDir, m_baseFileName).arg(i);
            QString dst = QStringLiteral("%1/%2.%3").arg(m_logDir, m_baseFileName).arg(i + 1);
            if (QFile::exists(src)) {
                QFile::rename(src, dst);
            }
        }

        QString firstBackup = QStringLiteral("%1/%2.1").arg(m_logDir, m_baseFileName);
        QFile::rename(currentLogFilePath(), firstBackup);

        openCurrentFile();
    }

    void RollingFileSink::write(const LogRecord &record) {
        QString line = LogFormatter::format(record, false) + QStringLiteral("\n");
        QByteArray bytes = line.toUtf8();

        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen() && !openCurrentFile()) {
            return;
        }

        m_file.write(bytes);
        m_currentFileSize += bytes.size();
        rotateIfNeeded();
    }

    void RollingFileSink::writeBatch(const std::vector<LogRecord> &records) {
        if (records.empty()) return;

        QString batchBuffer;
        batchBuffer.reserve(records.size() * 128);

        for (const auto &record : records) {
            batchBuffer += LogFormatter::format(record, false);
            batchBuffer += QStringLiteral("\n");
        }

        QByteArray bytes = batchBuffer.toUtf8();

        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen() && !openCurrentFile()) {
            return;
        }

        m_file.write(bytes);
        m_currentFileSize += bytes.size();
        rotateIfNeeded();
    }

    void RollingFileSink::flush() {
        QMutexLocker locker(&m_mutex);
        if (m_file.isOpen()) {
            m_file.flush();
        }
    }

} // namespace core::logging
