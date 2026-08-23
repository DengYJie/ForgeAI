#pragma once

#include "domain/service/IModelService.h"
#include <memory>

namespace core::model {
    class ModelRegistry;
}

namespace services::model {
    /**
     * @brief 模型服务实现（基于 core::model::ModelRegistry）
     */
    class ModelService : public domain::service::IModelService {
        Q_OBJECT

    public:
        explicit ModelService(
            std::shared_ptr<core::model::ModelRegistry> registry,
            QObject *parent = nullptr
        );

        ~ModelService() override = default;

        QList<domain::model::ModelProvider> getActiveProviders() const override;
        QList<domain::model::Model> getEnabledModels() const override;
        std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) const override;
        std::optional<std::pair<domain::model::Model, domain::model::ModelProvider>> resolveModel(const QString &modelId) const override;
        void saveProvider(const domain::model::ModelProvider &provider) override;
        void deleteProvider(const QString &providerId) override;

    private:
        std::shared_ptr<core::model::ModelRegistry> m_registry;
    };
} // namespace services::model
