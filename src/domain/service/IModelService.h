#pragma once

#include <QObject>
#include <QList>
#include <optional>
#include <utility>
#include "domain/model/Model.h"
#include "domain/model/ModelProvider.h"
#include "domain/model/ProviderModel.h"
#include "domain/model/ResolvedModel.h"

namespace domain::service {
    /**
     * @brief 模型与服务商管理服务接口
     */
    class IModelService : public QObject {
        Q_OBJECT

    public:
        using QObject::QObject;
        ~IModelService() override = default;

        virtual QList<domain::model::ModelProvider> getActiveProviders() const = 0;
        virtual QList<domain::model::Model> getEnabledModels() const = 0;
        virtual std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) const = 0;
        virtual std::optional<std::pair<domain::model::Model, domain::model::ModelProvider>> resolveModel(const QString &modelId) const = 0;
        virtual void saveProvider(const domain::model::ModelProvider &provider) = 0;
        virtual void deleteProvider(const QString &providerId) = 0;

        virtual QList<domain::model::ResolvedModel> getModelsForProvider(const QString &providerId) const = 0;
        virtual void saveProviderModel(const domain::model::ProviderModel &binding) = 0;
        virtual void deleteProviderModel(const QString &providerId, const QString &remoteModelId) = 0;

    Q_SIGNALS:
        void providersChanged();
        void modelsChanged();
    };
} // namespace domain::service
