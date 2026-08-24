#include "ModelSettingsPageFactory.h"

#include <FluentQt/Design.h>

#include "ui/screen/settings/model_manager/ModelManagerPage.h"

namespace ui::screen::settings {
    ModelSettingsPageFactory::ModelSettingsPageFactory(model_manager::ModelManagerViewModel *managerViewModel)
        : m_managerViewModel(managerViewModel) {
    }

    QString ModelSettingsPageFactory::category() const {
        return QObject::tr("模型");
    }

    QString ModelSettingsPageFactory::title() const {
        return QObject::tr("模型与服务商");
    }

    QString ModelSettingsPageFactory::iconGlyph() const {
        return Typography::Icons::Cloud;
    }

    QWidget *ModelSettingsPageFactory::createProviderPage(QWidget *parent) {
        return new model_manager::ModelManagerPage(m_managerViewModel, parent);
    }
} // namespace ui::screen::settings
