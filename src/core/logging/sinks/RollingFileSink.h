#pragma once
#include "ILogSink.h"
#include <QString>
#include <QFile>
#include <QMutex>

namespace core::logging {

    class RollingFileSink : public ILogSink {
    public:
        explicit RollingFileSink(
            const QString &logDir = QString(),
            const QString &baseFileName = QStringLiteral("app.log"),
            qint64 maxFileSize = 10 * 1024 * 1024,
            int maxBackupFiles = 5
        );

        ~RollingFileSink() override;

        void write(const LogRecord &record) override;
        void writeBatch(const std::vector<LogRecord> &records) override;
        void flush() override;

        QString logDirectory() const { return m_logDir; }
        QString currentLogFilePath() const;
        QString backupFilePath(int index) const;

    private:
        bool openCurrentFile();
        void rotateIfNeeded();

        QString m_logDir;
        QString m_baseFileName;
        qint64 m_maxFileSize;
        int m_maxBackupFiles;

        qint64 m_currentFileSize = 0;
        QFile m_file;
        QMutex m_mutex;
    };

} // namespace core::logging
