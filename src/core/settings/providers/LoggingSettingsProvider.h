#pragma once
#include <QObject>
#include "core/settings/BaseSettingsProvider.h"
#include "core/logging/LogLevel.h"

namespace core::settings {

    /**
     * @brief 日志与诊断配置项持久化 Provider
     */
    class LoggingSettingsProvider : public BaseSettingsProvider {
        Q_OBJECT

    public:
        /**
         * @brief 日志级别配置键 (0: 普通 / Info+, 1: 详细 / Debug+, 2: 调试 / Trace+)
         */
        static inline const SettingKey<int> LogLevelKey{"logLevel", 0};

        explicit LoggingSettingsProvider(QObject *parent = nullptr);

        QString id() const override { return QStringLiteral("logging"); }
        QString category() const override { return QStringLiteral("日志与诊断"); }
        bool useSeparateFile() const override { return false; }
        QString configFileName() const override { return QString(); }

    protected:
        void onSettingChanged(const QString &key) override;
        void onSettingsLoaded() override;

    private:
        void applyLogLevel() const;
    };

} // namespace core::settings
