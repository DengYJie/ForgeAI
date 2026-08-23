#pragma once
#include "core/settings/BaseSettingsProvider.h"

namespace core::settings {

    /**
     * @brief 模型与服务商设置提供者
     * @details 提供模型模块在 Settings 系统中的 Provider 接入点
     */
    class ModelSettingsProvider : public BaseSettingsProvider {
        Q_OBJECT

    public:
        explicit ModelSettingsProvider(QObject *parent = nullptr);
        ~ModelSettingsProvider() override = default;

        QString id() const override;
        QString category() const override;
        bool useSeparateFile() const override { return false; }
        QString configFileName() const override { return ""; }
    };

} // namespace core::settings
