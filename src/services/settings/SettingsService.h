#pragma once

#include "domain/service/ISettingsService.h"

namespace services::settings {
    /**
     * @brief 全局应用设置服务实现（封装 core::settings::SettingsRegistry）
     */
    class SettingsService : public domain::service::ISettingsService {
        Q_OBJECT

    public:
        explicit SettingsService(QObject *parent = nullptr);
        ~SettingsService() override = default;

        void loadAll() override;
        void saveAllSync() override;
    };
} // namespace services::settings
