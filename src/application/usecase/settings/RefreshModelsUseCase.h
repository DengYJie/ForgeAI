#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include "application/ports/IModelDiscoveryGateway.h"

namespace core::model {
    class ModelRegistry;
}

namespace application::usecase::settings {

    /**
     * @brief 探测并刷新服务商可用模型列表用例
     * @details 负责向远端拉取模型列表，与本地注册表进行元数据 Hydration 匹配，并持久化到 SQLite
     */
    class RefreshModelsUseCase : public QObject {
        Q_OBJECT

    public:
        explicit RefreshModelsUseCase(
            ports::IModelDiscoveryGateway *discoveryGateway,
            std::shared_ptr<core::model::ModelRegistry> registry,
            QObject *parent = nullptr
        );

        ~RefreshModelsUseCase() override;

        /**
         * @brief 触发针对指定 Provider 的模型探测
         * @param providerId 服务商唯一标识
         */
        void execute(const QString &providerId);

        /**
         * @brief 取消当前的探测请求
         */
        void cancel();

        /**
         * @brief 是否正在执行探测中
         */
        bool isDiscovering() const;

    Q_SIGNALS:
        void discoveryStarted(const QString &providerId);
        void discoveryFinished(const QString &providerId, int modelCount);
        void discoveryFailed(const QString &providerId, const QString &errorMessage);

    private Q_SLOTS:
        void onModelsFetched(const QList<domain::model::Model> &models);
        void onFetchFailed(const QString &errorMessage);

    private:
        ports::IModelDiscoveryGateway *m_discoveryGateway;
        std::shared_ptr<core::model::ModelRegistry> m_registry;

        ports::IModelDiscoveryOperation *m_currentOp = nullptr;
        QString m_currentProviderId;
    };

} // namespace application::usecase::settings
