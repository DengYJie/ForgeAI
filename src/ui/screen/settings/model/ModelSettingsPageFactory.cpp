#include "ModelSettingsPageFactory.h"

#include <FluentQt/Design.h>

#include "ui/screen/settings/model_manager/ModelManagerPage.h"

namespace ui::screen::settings {
    ModelSettingsPageFactory::ModelSettingsPageFactory(model_manager::ModelManagerViewModel *managerViewModel)
        : m_managerViewModel(managerViewModel) {
    }

    QString ModelSettingsPageFactory::iconGlyph() const {
        return Typography::Icons::Cloud;
    }

    QWidget *ModelSettingsPageFactory::createProviderPage(QWidget *parent) {
        return new model_manager::ModelManagerPage(m_managerViewModel, parent);
    }
} // namespace ui::screen::settings
