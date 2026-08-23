#include "LoggingSettingsProvider.h"
#include "core/logging/LoggingService.h"

namespace core::settings {

    LoggingSettingsProvider::LoggingSettingsProvider(QObject *parent)
        : BaseSettingsProvider(parent) {
    }

    void LoggingSettingsProvider::onSettingChanged(const QString &key) {
        if (key == LogLevelKey.name) {
            applyLogLevel();
        }
    }

    void LoggingSettingsProvider::onSettingsLoaded() {
        applyLogLevel();
    }

    void LoggingSettingsProvider::applyLogLevel() const {
        int levelIndex = get(LogLevelKey);
        core::logging::LogLevel targetLevel = core::logging::LogLevel::Info;

        switch (levelIndex) {
            case 0:
                targetLevel = core::logging::LogLevel::Info;    // 普通 -> Info+
                break;
            case 1:
                targetLevel = core::logging::LogLevel::Debug;   // 详细 -> Debug+
                break;
            case 2:
                targetLevel = core::logging::LogLevel::Trace;   // 调试 -> Trace+
                break;
            default:
                targetLevel = core::logging::LogLevel::Info;
                break;
        }

        core::logging::LoggingService::instance().setMinLevel(targetLevel);
    }

} // namespace core::settings
