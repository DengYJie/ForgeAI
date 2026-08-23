#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <vector>

namespace domain::service {
    /**
     * @brief 全局应用设置服务接口
     */
    class ISettingsService : public QObject {
        Q_OBJECT

    public:
        using QObject::QObject;
        ~ISettingsService() override = default;

        /**
         * @brief 加载所有配置项
         */
        virtual void loadAll() = 0;

        /**
         * @brief 同步保存所有发生变更的配置项
         */
        virtual void saveAllSync() = 0;

    Q_SIGNALS:
        /**
         * @brief 设置发生变更通知
         * @param providerId 发生变更的 Provider 标识
         */
        void settingsChanged(const QString &providerId);
    };
} // namespace domain::service
