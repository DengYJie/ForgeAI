#pragma once

#include "ui/base/BaseViewModel.h"
#include "domain/model/ModelProvider.h"
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
        bool isLoading = false;
        bool isRefreshing = false;
        QString refreshingProviderId;
        QString errorMessage;

        bool operator==(const ModelManagerState &other) const = default;
    };

    /**
     * @brief 模型管理器 ViewModel (UDF 架构核心)，协调 UseCases 与 UI State
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

        /**
         * @brief 加载/重新加载全部服务商列表
         */
        void loadProviders();

        /**
         * @brief 选中指定服务商
         */
        void selectProvider(const QString &providerId);

        /**
         * @brief 暂存并保存服务商配置变更（支持即时或后续批量提交）
         */
        void saveProvider(const domain::model::ModelProvider &provider);

        /**
         * @brief 删除指定服务商并自动选中临近项
         */
        void deleteProvider(const QString &providerId);

        /**
         * @brief 探测并刷新服务商可用模型列表
         */
        void refreshModels(const QString &providerId);

        /**
         * @brief 新增服务商并自动切换选中
         */
        void addProvider(const domain::model::ModelProvider &provider);

        /**
         * @brief 为指定服务商添加自定义模型
         */
        void addModel(const QString &providerId, const domain::model::Model &model);

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
