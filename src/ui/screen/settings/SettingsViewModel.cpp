#include "SettingsViewModel.h"
#include "ui/screen/settings/model_manager/ModelManagerDialog.h"

namespace ui::screen::settings {
    SettingsViewModel::SettingsViewModel(
        const application::usecase::settings::SettingsUseCases &useCases,
        QObject *parent
    ) : BaseViewModel<SettingsViewModel, SettingsState>(parent),
        m_useCases(useCases) {
        setupUseCaseConnections();
        loadAll();
    }

    SettingsViewModel::~SettingsViewModel() = default;

    void SettingsViewModel::setupUseCaseConnections() {
        if (m_useCases.getModels) {
            connect(m_useCases.getModels, &application::usecase::settings::GetModelsUseCase::modelsChanged,
                    this, &SettingsViewModel::refreshModels);
        }
    }

    void SettingsViewModel::loadAll() {
        if (m_useCases.loadSettings) {
            m_useCases.loadSettings->execute();
        }
        refreshModels();
    }

    void SettingsViewModel::saveAll() {
        if (m_useCases.saveSetting) {
            m_useCases.saveSetting->execute();
        }
        updateState([](SettingsState &s) {
            s.statusMessage = QStringLiteral("设置已保存");
        });
    }

    void SettingsViewModel::refreshModels() {
        if (m_useCases.getModels) {
            auto providers = m_useCases.getModels->getActiveProviders();
            auto models = m_useCases.getModels->getEnabledModels();
            updateState([providers, models](SettingsState &s) {
                s.providers = providers;
                s.enabledModels = models;
            });
        }
    }

    void SettingsViewModel::openModelManager(QWidget *parent) {
        auto *dialog = new model_manager::ModelManagerDialog(
            m_useCases.modelRegistry.get(),
            m_useCases.refreshModels,
            parent
        );
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(dialog, &QDialog::finished, this, &SettingsViewModel::refreshModels);
        dialog->exec();
    }

    void SettingsViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::settings
