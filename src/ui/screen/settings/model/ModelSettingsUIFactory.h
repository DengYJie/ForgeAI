#pragma once
#include "ui/screen/settings/ISettingsUIFactory.h"
#include "ModelSettingsViewModel.h"

namespace ui::screen::settings {
    /**
     * @brief 模型与服务商管理设置卡片 UI 工厂
     */
    class ModelSettingsUIFactory : public ISettingsUIFactory {
    public:
        /**
         * @param viewModel 模型设置局部 ViewModel 指针
         */
        explicit ModelSettingsUIFactory(ModelSettingsViewModel *viewModel);
        ~ModelSettingsUIFactory() override = default;

        QString id() const override { return QStringLiteral("model.manager"); }
        QString categoryId() const override { return QStringLiteral("model"); }
        QString categoryDisplayName() const override;
        int categoryOrder() const override { return 0; }
        int itemOrder() const override { return 0; }

        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent = nullptr) override;

    private:
        ModelSettingsViewModel *m_viewModel = nullptr;
    };
} // namespace ui::screen::settings
