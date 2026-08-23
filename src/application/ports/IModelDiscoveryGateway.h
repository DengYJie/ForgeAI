#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include "domain/model/Model.h"
#include "domain/model/ModelProvider.h"

namespace application::ports {

    /**
     * @brief 异步模型发现操作句柄
     */
    class IModelDiscoveryOperation : public QObject {
        Q_OBJECT
    public:
        explicit IModelDiscoveryOperation(QObject *parent = nullptr) : QObject(parent) {}
        virtual ~IModelDiscoveryOperation() = default;

        virtual void cancel() = 0;

    Q_SIGNALS:
        void finished(const QList<domain::model::Model> &models);
        void failed(const QString &errorMessage);
    };

    /**
     * @brief 抽象模型发现网关接口
     */
    class IModelDiscoveryGateway {
    public:
        virtual ~IModelDiscoveryGateway() = default;

        /**
         * @brief 向指定 Provider 发起模型列表拉取请求
         * @param provider 目标服务商配置
         * @return 操作句柄，调用方负责生命周期管理
         */
        virtual IModelDiscoveryOperation* fetchModels(const domain::model::ModelProvider &provider) = 0;
    };

} // namespace application::ports
