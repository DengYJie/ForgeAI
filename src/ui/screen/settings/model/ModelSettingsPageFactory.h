#pragma once

#include "ui/screen/settings/ISettingsUIFactory.h"

namespace ui::screen::settings::model_manager {
    class ModelManagerViewModel;
}

namespace ui::screen::settings {
    class ModelSettingsPageFactory : public ISettingsProviderPageFactory {
    public:
        explicit ModelSettingsPageFactory(model_manager::ModelManagerViewModel *managerViewModel);
        ~ModelSettingsPageFactory() override = default;

        QString providerId() const override { return QStringLiteral("model"); }
        QString category() const override;
        QString title() const override;
        int categoryOrder() const override { return 0; }
        int order() const override { return 0; }
        QString iconGlyph() const override;
        QWidget *createProviderPage(QWidget *parent) override;

    private:
        model_manager::ModelManagerViewModel *m_managerViewModel = nullptr;
    };
} // namespace ui::screen::settings
