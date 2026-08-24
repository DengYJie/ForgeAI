#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <memory>
#include <optional>

#include "ui/navigation/INavigationRegistrar.h"
#include "ui/screen/settings/SettingsDescriptorBuilder.h"

class QBoxLayout;

namespace ui::navigation {
    class NavigationPanel;
}

namespace ui::screen::settings {

    class SettingsUIRegistry;

    /**
     * @brief 设置导航特性安装模块
     * @details 负责将设置体系（Surface、Provider 路由、导航项）编排并安装至 INavigationRegistrar
     */
    class SettingsNavigationModule : public QObject {
        Q_OBJECT

    public:
        explicit SettingsNavigationModule(
            SettingsUIRegistry *uiRegistry = nullptr,
            QObject *parent = nullptr
        );

        ~SettingsNavigationModule() override;

        /**
         * @brief 将设置特性安装到指定的导航注册器
         * @param registrar 导航注册器引用
         * @return 安装是否成功
         */
        bool install(ui::navigation::INavigationRegistrar &registrar);

        /**
         * @brief 获取首个 Provider 路由键
         */
        QString initialRoute() const { return m_initialRouteKey; }

        /**
         * @brief 获取主导航栏入口项描述符（若无有效设置项则返回 std::nullopt）
         */
        std::optional<ui::navigation::NavigationItemDescriptor> entryDescriptor() const;

        /**
         * @brief 响应式刷新边距与 Card 堆叠
         */
        void updateResponsiveLayout(int availableWidth);

    private:
        QString routeKeyForProvider(const QString &providerId) const;

        SettingsUIRegistry *m_uiRegistry = nullptr;
        bool m_isInstalled = false;

        std::unique_ptr<ui::navigation::NavigationPanel> m_panel;
        ui::navigation::NavigationPanel *m_installedPanel = nullptr;

        QString m_initialRouteKey;
        QHash<QString, SettingsProviderPageDescriptor> m_routeToDescriptor;
        QHash<QString, QString> m_providerToRoute;

        QList<QBoxLayout *> m_pageLayouts;
        QList<QWidget *> m_cards;
    };

} // namespace ui::screen::settings
