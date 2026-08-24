#pragma once

#include "ui/base/BaseViewModel.h"
#include "domain/model/ModelProvider.h"
#include "domain/model/ResolvedModel.h"
#include "domain/model/ProviderModel.h"
#include <QString>
#include <QList>
#include <optional>

namespace application::usecase::settings {
    class GetModelsUseCase;
    class SaveProviderUseCase;
    class DeleteProviderUseCase;
    class RefreshModelsUseCase;
}

namespace ui::screen::settings::model_manager {

    /**
     * @brief 模型管理器不可变状态快照 (UDF State)
     */
    struct ModelManagerState {
        QList<domain::model::ModelProvider> providers;
        QString selectedProviderId;
        std::optional<domain::model::ModelProvider> selectedProvider;
        QList<domain::model::ResolvedModel> selectedProviderModels;
        bool isLoading = false;
        bool isRefreshing = false;
        QString refreshingProviderId;
        QString errorMessage;

        bool operator==(const ModelManagerState &other) const = default;
    };

    /**
     * @brief 模型管理器 ViewModel
     */
    class ModelManagerViewModel : public BaseViewModel<ModelManagerViewModel, ModelManagerState> {
        Q_OBJECT

    public:
        explicit ModelManagerViewModel(
            application::usecase::settings::GetModelsUseCase *getModelsUseCase,
            application::usecase::settings::SaveProviderUseCase *saveProviderUseCase,
            application::usecase::settings::DeleteProviderUseCase *deleteProviderUseCase,
            application::usecase::settings::RefreshModelsUseCase *refreshModelsUseCase,
            QObject *parent = nullptr
        );

        ~ModelManagerViewModel() override = default;

        void loadProviders();
        void selectProvider(const QString &providerId);
        void saveProvider(const domain::model::ModelProvider &provider);
        void deleteProvider(const QString &providerId);
        void refreshModels(const QString &providerId);
        void addProvider(const domain::model::ModelProvider &provider);
        void addModel(const domain::model::ProviderModel &binding);

    Q_SIGNALS:
        void stateChanged(const ui::screen::settings::model_manager::ModelManagerState &state);
        void toastRequested(const QString &message, bool isError);

    protected:
        void emitStateChanged() override {
            Q_EMIT stateChanged(m_state);
        }

    private:
        void applyProviderSelection(ModelManagerState &state, const QString &preferredId = QString());

        application::usecase::settings::GetModelsUseCase *m_getModelsUseCase = nullptr;
        application::usecase::settings::SaveProviderUseCase *m_saveProviderUseCase = nullptr;
        application::usecase::settings::DeleteProviderUseCase *m_deleteProviderUseCase = nullptr;
        application::usecase::settings::RefreshModelsUseCase *m_refreshModelsUseCase = nullptr;
    };

} // namespace ui::screen::settings::model_manager
