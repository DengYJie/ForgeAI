#pragma once

#include <QObject>
#include <QList>
#include "domain/model/Model.h"
#include "domain/model/ModelProvider.h"

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

        /**
         * @brief 获取所有已激活的服务商配置列表
         */
        QList<domain::model::ModelProvider> getActiveProviders() const;

        /**
         * @brief 获取所有已启用的模型列表
         */
        QList<domain::model::Model> getEnabledModels() const;

    Q_SIGNALS:
        void modelsChanged();

    private:
        domain::service::IModelService *m_modelService = nullptr;
    };
} // namespace application::usecase::settings
