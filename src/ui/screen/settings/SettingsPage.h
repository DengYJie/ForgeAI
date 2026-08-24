#pragma once

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include "ui/screen/settings/ISettingsUIFactory.h"

class QVBoxLayout;
class NavigationWindow;

namespace fluent::navigation {
    class NavigationView;
}

namespace fluent::textfields {
    class Label;
}

namespace ui::navigation {
    class NavigationPanel;
}

namespace ui::screen::settings {
    class SettingsCardItem;
    class SettingsViewModel;
    class SettingsUIRegistry;
    class SettingsCoordinator;
    struct SettingsState;

    struct SettingsProviderPageDescriptor {
        QString providerId;
        QString category;
        QString title;
        QString iconGlyph;
        int order = 1000;
        QList<std::shared_ptr<ISettingsUIFactory>> factories;
        ISettingsProviderPageFactory *pageFactory = nullptr;

        bool hasCustomPage() const { return pageFactory != nullptr; }
    };

    /**
     * @brief 设置路由与设置导航控制器
     * @details settings 本身不再注册为内容页；该对象负责注册 provider 页面路由，并在进入 settings.provider.* 路由时替换主导航面板
     */
    class SettingsPage : public QObject {
        Q_OBJECT

    public:
        /**
         * @param viewModel 页面级 ViewModel
         * @param uiRegistry 设置 UI 工厂注册中心
         * @param coordinator 表现层协调者
         * @param parent 宿主父控件
         */
        explicit SettingsPage(
            SettingsViewModel *viewModel = nullptr,
            SettingsUIRegistry *uiRegistry = nullptr,
            SettingsCoordinator *coordinator = nullptr,
            fluent::navigation::NavigationView *navigationView = nullptr,
            ui::navigation::NavigationPanel *globalNavigationPanel = nullptr,
            QObject *parent = nullptr
        );

        ~SettingsPage() override = default;

        /**
         * @brief 获取关联的页面级 ViewModel
         */
        SettingsViewModel *viewModel() const { return m_viewModel; }

        /**
         * @brief 获取关联的表现层协调者
         */
        SettingsCoordinator *coordinator() const { return m_coordinator; }

        void registerProviderRoutes(NavigationWindow *window);
        QString initialProviderRouteKey() const { return m_initialRouteKey; }
        void handleRouteChanged(const QString &routeKey);

        /**
         * @brief 刷新已创建 provider 页面的响应式边距
         */
        void updateResponsiveLayout(int availableWidth);

    private:
        void setupUi();
        void render(const SettingsState &state);
        QList<SettingsProviderPageDescriptor> buildProviderPageDescriptors() const;
        QWidget *createGenericProviderPage(const SettingsProviderPageDescriptor &descriptor, QWidget *parent);
        QWidget *createCustomProviderPage(const SettingsProviderPageDescriptor &descriptor, QWidget *parent);
        QWidget *createSectionHeader(const QString &title, QWidget *parent);
        QWidget *createSettingsCard(const QString &iconGlyph,
                                    const QString &title,
                                    const QString &subtitle,
                                    QWidget *trailingWidget,
                                    QWidget *parent);
        QString routeKeyForProvider(const QString &providerId) const;
        void selectProviderPage(const QString &providerId);
        void attachSettingsNavigation();
        void detachSettingsNavigation();
        void bindSettingsNavigationToNavView();
        void unbindSettingsNavigationFromNavView();
        void syncSettingsNavigationWithNavView();

        SettingsViewModel *m_viewModel = nullptr;
        SettingsUIRegistry *m_uiRegistry = nullptr;
        SettingsCoordinator *m_coordinator = nullptr;
        fluent::navigation::NavigationView *m_navigationView = nullptr;
        ui::navigation::NavigationPanel *m_globalNavigationPanel = nullptr;
        ui::navigation::NavigationPanel *m_settingsNavigationPanel = nullptr;
        NavigationWindow *m_navigationWindow = nullptr;

        QPointer<QWidget> m_detachedMainChromeWidget;
        QHash<QString, SettingsProviderPageDescriptor> m_routeToDescriptor;
        QHash<QString, QString> m_providerToRoute;
        QString m_initialRouteKey;
        QList<QVBoxLayout *> m_pageLayouts;
        QList<SettingsCardItem *> m_cards;
        QList<QMetaObject::Connection> m_navigationViewConnections;
        bool m_settingsNavigationAttached = false;
    };
} // namespace ui::screen::settings
