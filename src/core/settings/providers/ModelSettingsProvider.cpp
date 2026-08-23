#include "ModelSettingsProvider.h"
#include "core/settings/ISettingsProvider.h"
#include "core/settings/SettingsRegistry.h"

namespace core::settings {

    ModelSettingsProvider::ModelSettingsProvider(QObject *parent)
        : BaseSettingsProvider(parent) {
    }

    QString ModelSettingsProvider::id() const {
        return QStringLiteral("model");
    }

    QString ModelSettingsProvider::category() const {
        return QStringLiteral("模型与服务商");
    }

} // namespace core::settings

REGISTER_SETTINGS_PROVIDER(core::settings::ModelSettingsProvider)
