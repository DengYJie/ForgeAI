#pragma once
#include "ui/screen/settings/ISettingsUIFactory.h"

namespace ui::screen::settings {

    /**
     * @brief 模型与服务商管理设置卡片 UI 工厂
     */
    class ModelManagerSettingsUIFactory : public ISettingsUIFactory {
    public:
        QString targetProviderId() const override { return QStringLiteral("model"); }
        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent = nullptr) override;
    };

} // namespace ui::screen::settings
