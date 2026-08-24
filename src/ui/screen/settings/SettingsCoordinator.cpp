#include "SettingsCoordinator.h"

namespace ui::screen::settings {
    SettingsCoordinator::SettingsCoordinator(
        model_manager::ModelManagerViewModel *viewModel,
        QObject *parent
    ) : QObject(parent),
        m_viewModel(viewModel) {
    }

    void SettingsCoordinator::openModelManager(QWidget *parent) {
        Q_UNUSED(parent);
        emit providerPageRequested(QStringLiteral("model"));
    }
} // namespace ui::screen::settings
