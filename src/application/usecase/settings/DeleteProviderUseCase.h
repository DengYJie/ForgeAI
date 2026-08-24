#pragma once

#include <QObject>
#include <QString>

namespace domain::service {
    class IModelService;
}

namespace application::usecase::settings {
    /**
     * @brief 删除模型服务商配置用例
     */
    class DeleteProviderUseCase : public QObject {
        Q_OBJECT

    public:
        explicit DeleteProviderUseCase(
            domain::service::IModelService *modelService,
            QObject *parent = nullptr
        );

        ~DeleteProviderUseCase() override = default;

        /**
         * @brief 执行服务商删除
         */
        void execute(const QString &providerId);

    private:
        domain::service::IModelService *m_modelService = nullptr;
    };
} // namespace application::usecase::settings
