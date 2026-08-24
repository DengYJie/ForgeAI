#pragma once

#include <QObject>
#include <QList>
#include "domain/model/Model.h"
#include "domain/model/ModelProvider.h"
#include "domain/model/ResolvedModel.h"

namespace domain::service {
    class IModelService;
}

namespace application::usecase::settings {
    /**
     * @brief 获取模型与服务商配置用例
     */
    class GetModelsUseCase : public QObject {
        Q_OBJECT

    public:
        explicit GetModelsUseCase(
            domain::service::IModelService *modelService,
            QObject *parent = nullptr
        );

        ~GetModelsUseCase() override = default;

        QList<domain::model::ModelProvider> getActiveProviders() const;
        QList<domain::model::ResolvedModel> getModelsForProvider(const QString &providerId) const;
        QList<domain::model::Model> getEnabledModels() const;

    Q_SIGNALS:
        void modelsChanged();

    private:
        domain::service::IModelService *m_modelService = nullptr;
    };
} // namespace application::usecase::settings
