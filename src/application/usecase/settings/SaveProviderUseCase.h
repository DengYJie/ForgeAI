#pragma once

#include <QObject>
#include "domain/model/ModelProvider.h"

namespace domain::service {
    class IModelService;
}

namespace application::usecase::settings {
    /**
     * @brief 保存/更新模型服务商配置用例
     */
    class SaveProviderUseCase : public QObject {
        Q_OBJECT

    public:
        explicit SaveProviderUseCase(
            domain::service::IModelService *modelService,
            QObject *parent = nullptr
        );

        ~SaveProviderUseCase() override = default;

        /**
         * @brief 执行服务商持久化保存
         */
        void execute(const domain::model::ModelProvider &provider);

    private:
        domain::service::IModelService *m_modelService = nullptr;
    };
} // namespace application::usecase::settings
