#pragma once
#include <memory>
#include <QList>
#include "ISettingsUIFactory.h"

namespace ui::screen::settings {
    /**
     * @brief 设置界面 UI 工厂注册中心
     * @details 负责收集管理所有 ISettingsUIFactory 实例，并提供按分类/项顺序排序的工厂列表供 SettingsPage 渲染
     */
    class SettingsUIRegistry {
    public:
        explicit SettingsUIRegistry() = default;
        ~SettingsUIRegistry() = default;

        /**
         * @brief 注册设置项 UI 工厂
         * @param factory 工厂共享指针
         */
        void registerFactory(std::shared_ptr<ISettingsUIFactory> factory);

        /**
         * @brief 获取所有已注册的 UI 工厂原始列表
         * @return 工厂列表
         */
        QList<std::shared_ptr<ISettingsUIFactory>> allFactories() const;

        /**
         * @brief 获取按 categoryOrder、categoryId、itemOrder 严格排序后的工厂列表
         * @return 已排序的工厂列表
         */
        QList<std::shared_ptr<ISettingsUIFactory>> sortedFactories() const;

    private:
        QList<std::shared_ptr<ISettingsUIFactory>> m_factories;
    };
} // namespace ui::screen::settings
