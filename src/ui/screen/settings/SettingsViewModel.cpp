#include "SettingsViewModel.h"

namespace ui::screen::settings {
    SettingsViewModel::SettingsViewModel(
        const application::usecase::settings::SettingsUseCases &useCases,
        QObject *parent
    ) : BaseViewModel<SettingsViewModel, SettingsState>(parent),
        m_useCases(useCases) {
        loadAll();
    }

    SettingsViewModel::~SettingsViewModel() = default;

    void SettingsViewModel::loadAll() {
        updateState([](SettingsState &s) {
            s.isLoading = true;
        });

        if (m_useCases.loadSettings) {
            m_useCases.loadSettings->execute();
        }

        updateState([](SettingsState &s) {
            s.isLoading = false;
        });
    }

    void SettingsViewModel::saveAll() {
        if (m_useCases.saveSetting) {
            m_useCases.saveSetting->execute();
        }
        updateState([](SettingsState &s) {
            s.statusMessage = QStringLiteral("设置已保存");
        });
    }

    void SettingsViewModel::setStatusMessage(const QString &message) {
        updateState([message](SettingsState &s) {
            s.statusMessage = message;
        });
    }

    void SettingsViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::settings
