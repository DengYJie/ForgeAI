#include "ModelSettingsProvider.h"

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

    QString ModelSettingsProvider::title() const {
        return QStringLiteral("模型与服务商");
    }

} // namespace core::settings
