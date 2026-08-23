#pragma once

#include "domain/service/ISettingsService.h"

namespace core::settings {
    class SettingsRegistry;
}

namespace services::settings {
    /**
     * @brief 全局应用设置服务实现类
     * @details 封装基础设施层 `core::settings::SettingsRegistry`，向领域层暴露标准配置加载与持久化操作
     */
    class SettingsService : public domain::service::ISettingsService {
        Q_OBJECT

    public:
        /**
         * @param registry 设置注册中心指针
         * @param parent 父 QObject
         */
        explicit SettingsService(core::settings::SettingsRegistry *registry, QObject *parent = nullptr);
        ~SettingsService() override = default;

        /**
         * @brief 触发全量设置加载
         */
        void loadAll() override;

        /**
         * @brief 触发全量设置同步保存
         */
        void saveAllSync() override;

    private:
        core::settings::SettingsRegistry *m_registry = nullptr;
    };
} // namespace services::settings
