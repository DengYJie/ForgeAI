#include "LoggingSettingsViewModel.h"
#include "core/settings/providers/LoggingSettingsProvider.h"
#include "core/logging/LoggingSettingsService.h"

namespace ui::screen::settings {
    LoggingSettingsViewModel::LoggingSettingsViewModel(
        core::settings::LoggingSettingsProvider *provider,
        core::logging::LoggingSettingsService *service,
        QObject *parent
    ) : QObject(parent), m_provider(provider), m_service(service) {
        if (!m_service) {
            m_service = &core::logging::LoggingSettingsService::instance();
        }

        if (m_provider) {
            connect(m_provider, &core::settings::ISettingsProvider::dataChanged, this, [this]() {
                emit logLevelChanged(logLevel());
            });
        }

        if (m_service) {
            connect(m_service, &core::logging::LoggingSettingsService::logSizeChanged, this, [this](qint64) {
                emit logSizeChanged(formattedLogSize());
            });
        }
    }

    int LoggingSettingsViewModel::logLevel() const {
        if (!m_provider) {
            return 0;
        }
        return m_provider->get(core::settings::LoggingSettingsProvider::LogLevelKey);
    }

    void LoggingSettingsViewModel::setLogLevel(int level) {
        if (m_provider) {
            m_provider->set(core::settings::LoggingSettingsProvider::LogLevelKey, level);
        }
    }

    QString LoggingSettingsViewModel::formattedLogSize() const {
        if (m_service) {
            return m_service->getFormattedLogSize();
        }
        return QStringLiteral("0.0 KB");
    }

    bool LoggingSettingsViewModel::clearLogs() {
        if (m_service) {
            bool ok = m_service->clearLogs();
            if (ok) {
                emit logSizeChanged(formattedLogSize());
            }
            return ok;
        }
        return false;
    }

    void LoggingSettingsViewModel::openLogDirectory() {
        if (m_service) {
            m_service->openLogDirectory();
        }
    }

    bool LoggingSettingsViewModel::exportDiagnostics(const QString &destinationZipPath) {
        if (m_service) {
            return m_service->exportDiagnostics(destinationZipPath);
        }
        return false;
    }
} // namespace ui::screen::settings
