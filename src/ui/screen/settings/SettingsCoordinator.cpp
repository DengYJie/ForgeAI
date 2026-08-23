#include "SettingsCoordinator.h"
#include "ui/screen/settings/model_manager/ModelManagerDialog.h"

namespace ui::screen::settings {
    SettingsCoordinator::SettingsCoordinator(
        core::model::ModelRegistry *modelRegistry,
        application::usecase::settings::RefreshModelsUseCase *refreshUseCase,
        QObject *parent
    ) : QObject(parent),
        m_modelRegistry(modelRegistry),
        m_refreshUseCase(refreshUseCase) {
    }

    void SettingsCoordinator::openModelManager(QWidget *parent) {
        model_manager::ModelManagerDialog dialog(
            m_modelRegistry,
            m_refreshUseCase,
            parent
        );
        dialog.exec();
    }
} // namespace ui::screen::settings
