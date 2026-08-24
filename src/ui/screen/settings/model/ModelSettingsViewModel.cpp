#include "ModelSettingsViewModel.h"

namespace ui::screen::settings {
    ModelSettingsViewModel::ModelSettingsViewModel(QObject *parent)
        : QObject(parent) {
    }

    void ModelSettingsViewModel::requestOpenModelManager(QWidget *parent) {
        Q_EMIT modelManagerRequested(parent);
    }
} // namespace ui::screen::settings
