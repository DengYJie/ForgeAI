#pragma once
#include <QList>
#include <optional>
#include "domain/model/ModelProvider.h"
#include "domain/model/CanonicalModel.h"
#include "domain/model/ProviderModel.h"
#include "domain/model/ResolvedModel.h"

namespace domain::repository {

    /**
     * @brief 模型与服务商配置仓储接口
     */
    class IModelRepository {
    public:
        virtual ~IModelRepository() = default;

        virtual QList<domain::model::ModelProvider> getAllProviders() = 0;
        virtual QList<domain::model::ModelProvider> getEnabledProviders() = 0;
        virtual std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) = 0;
        virtual void saveProvider(const domain::model::ModelProvider &provider) = 0;
        virtual void deleteProvider(const QString &providerId) = 0;

        virtual std::optional<domain::model::CanonicalModel> getCanonicalModel(const QString &modelId) = 0;
        virtual QList<domain::model::CanonicalModel> getAllCanonicalModels() = 0;

        virtual QList<domain::model::ProviderModel> getProviderModels(const QString &providerId) = 0;
        virtual void saveProviderModel(const domain::model::ProviderModel &binding) = 0;
        virtual void deleteProviderModel(const QString &providerId, const QString &remoteModelId) = 0;

        virtual QList<domain::model::ResolvedModel> getResolvedModelsForProvider(const QString &providerId) = 0;
        virtual QList<domain::model::ResolvedModel> getAllResolvedModels() = 0;
        virtual QList<domain::model::ResolvedModel> getEnabledResolvedModels() = 0;
        virtual std::optional<domain::model::ResolvedModel> resolveModel(const QString &providerId, const QString &remoteModelId) = 0;
    };

} // namespace domain::repository
