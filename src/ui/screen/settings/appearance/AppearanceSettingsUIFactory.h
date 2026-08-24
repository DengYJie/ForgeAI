#pragma once
#include "ui/screen/settings/ISettingsUIFactory.h"
#include "AppearanceSettingsViewModel.h"

namespace ui::screen::settings {
    /**
     * @brief 应用主题设置卡片 UI 工厂
     */
    class AppearanceSettingsUIFactory : public ISettingsUIFactory {
    public:
        /**
         * @param viewModel 外观设置局部 ViewModel 指针
         */
        explicit AppearanceSettingsUIFactory(AppearanceSettingsViewModel *viewModel);
        ~AppearanceSettingsUIFactory() override = default;

        QString id() const override { return QStringLiteral("appearance.theme"); }
        QString providerId() const override { return QStringLiteral("appearance"); }
        QString categoryId() const override { return QStringLiteral("appearance"); }
        QString categoryDisplayName() const override;
        int categoryOrder() const override { return 10; }
        int itemOrder() const override { return 0; }

        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;

        QWidget *createControlWidget(QWidget *parent) override;

    private:
        AppearanceSettingsViewModel *m_viewModel = nullptr;
    };
} // namespace ui::screen::settings
