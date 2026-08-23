#include "ModelSettingsViewModel.h"

namespace ui::screen::settings {
    ModelSettingsViewModel::ModelSettingsViewModel(QObject *parent)
        : QObject(parent) {
    }

    void ModelSettingsViewModel::requestOpenModelManager() {
        emit modelManagerRequested();
    }
} // namespace ui::screen::settings
