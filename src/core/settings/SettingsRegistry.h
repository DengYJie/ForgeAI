#pragma once
#include <QObject>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QJsonObject>
#include <memory>
#include "ISettingsProvider.h"

class QFileSystemWatcher;

namespace core::settings {
    /**
     * @brief 设置注册与持久化管理中心
     * @details 负责管理所有 ISettingsProvider 实例的生命周期、聚合加载、防抖异步落盘以及外部文件变更热重载
     */
    class SettingsRegistry : public QObject {
        Q_OBJECT

    public:
        explicit SettingsRegistry(QObject *parent = nullptr);
        ~SettingsRegistry() override;

        /**
         * @brief 注册设置提供者
         * @param provider 设置提供者共享指针
         */
        void registerProvider(std::shared_ptr<ISettingsProvider> provider);

        /**
         * @brief 根据唯一标识符获取设置提供者
         * @param id 设置提供者 ID (如 "appearance", "logging", "model")
         * @return 匹配的提供者共享指针，若未找到则返回 nullptr
         */
        std::shared_ptr<ISettingsProvider> getProvider(const QString &id) const;

        /**
         * @brief 获取所有已注册的设置提供者映射表
         * @return providerId -> ISettingsProvider 映射表
         */
        QMap<QString, std::shared_ptr<ISettingsProvider>> allProviders() const { return m_providers; }

        /**
         * @brief 全量从磁盘加载所有配置数据
         * @details 自动创建配置目录，读取 config.json 及各独立配置文件并分发反序列化
         */
        void loadAll();

        /**
         * @brief 立即同步强制保存所有设置数据至磁盘
         */
        void saveAllSync();

    private slots:
        void onProviderDataChanged();
        void flushDirtyProviders();
        void onFileSystemChanged();
        void doReload();

    private:
        QString getMainConfigPath() const;
        QString getConfigDirPath() const;

        QMap<QString, std::shared_ptr<ISettingsProvider>> m_providers;
        QSet<ISettingsProvider *> m_dirtyProviders;

        QJsonObject m_cachedMainConfig;
        QTimer *m_saveTimer = nullptr;
        QTimer *m_reloadTimer = nullptr;
        QFileSystemWatcher *m_watcher = nullptr;
        bool m_isSaving = false;
    };
} // namespace core::settings
