#include "LoggingSettingsService.h"
#include "LoggingService.h"
#include "DiagnosticExporter.h"
#include "LogCategory.h"
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

namespace core::logging {

    LoggingSettingsService &LoggingSettingsService::instance() {
        static LoggingSettingsService s_instance;
        return s_instance;
    }

    LoggingSettingsService::LoggingSettingsService(QObject *parent)
        : QObject(parent) {
    }

    QString LoggingSettingsService::getLogDirectory() const {
        return QDir::homePath() + QStringLiteral("/.forgeai/logs");
    }

    qint64 LoggingSettingsService::getLogDirectorySizeBytes() const {
        QDir dir(getLogDirectory());
        if (!dir.exists()) {
            return 0;
        }

        qint64 total = 0;
        const auto entries = dir.entryInfoList(QDir::Files | QDir::NoSymLinks);
        for (const auto &info : entries) {
            total += info.size();
        }
        return total;
    }

    QString LoggingSettingsService::getFormattedLogSize() const {
        qint64 bytes = getLogDirectorySizeBytes();
        if (bytes <= 0) {
            return QStringLiteral("0.0 KB");
        }

        constexpr double KB = 1024.0;
        constexpr double MB = 1024.0 * 1024.0;
        constexpr double GB = 1024.0 * 1024.0 * 1024.0;

        if (bytes >= GB) {
            return QStringLiteral("%1 GB").arg(bytes / GB, 0, 'f', 1);
        }
        if (bytes >= MB) {
            return QStringLiteral("%1 MB").arg(bytes / MB, 0, 'f', 1);
        }
        return QStringLiteral("%1 KB").arg(bytes / KB, 0, 'f', 1);
    }

    bool LoggingSettingsService::clearLogs() {
        LoggingService::instance().flush();

        QDir dir(getLogDirectory());
        if (!dir.exists()) {
            Q_EMIT logSizeChanged(0);
            return true;
        }

        const auto entries = dir.entryInfoList(QDir::Files | QDir::NoSymLinks);
        for (const auto &info : entries) {
            if (info.fileName().startsWith(QStringLiteral("app."))) {
                QFile::remove(info.absoluteFilePath());
            }
        }

        LoggingService::instance().info(core::logging::Category::AppLifecycle, QStringLiteral("Logs cleared by user"));
        LoggingService::instance().flush();

        Q_EMIT logSizeChanged(getLogDirectorySizeBytes());
        return true;
    }

    void LoggingSettingsService::openLogDirectory() const {
        QString dirPath = getLogDirectory();
        QDir dir(dirPath);
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    }

    bool LoggingSettingsService::exportDiagnostics(
        const QString &destinationZipPath,
        const QList<domain::model::ModelProvider> &providers) const {
        
        return DiagnosticExporter::exportDiagnosticZip(destinationZipPath, providers);
    }

} // namespace core::logging
