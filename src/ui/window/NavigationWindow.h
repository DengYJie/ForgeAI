#pragma once

#include "ui/navigation/INavigationPresenter.h"
#include "ui/navigation/INavigationRegistrar.h"
#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationTypes.h"
#include "WindowBase.h"
#include <QHash>
#include <QString>
#include <QPointer>
#include <memory>

namespace ui::animation {
    class AnimatedVisualSource;
}

namespace ui::navigation {
    class NavigationPanel;
    class NavigationIndicator;
    class NavigationWidget;
}

namespace fluent::navigation {
    class NavigationView;
}

/**
 * @brief 导航窗口基类，作为多 Surface 托管与页面路由呈现的纯 UI Shell 容器
 */
class NavigationWindow : public WindowBase,
                         public ui::navigation::INavigationPresenter,
                         public ui::navigation::INavigationRegistrar {
    Q_OBJECT

public:
    explicit NavigationWindow(QWidget *parent = nullptr);
    ~NavigationWindow() override;

    bool registerSurface(const QString &surfaceId, ui::navigation::NavigationPanel *panel) override;
    bool registerRoute(const QString &routeKey, QWidget *page, const QString &surfaceId = QStringLiteral("main")) override;
    bool addNavigationItem(const ui::navigation::NavigationItemDescriptor &item, const QString &surfaceId = QStringLiteral("main")) override;

    bool setActiveNavigationSurface(const QString &surfaceId);
    ui::navigation::NavigationPanel *activeNavigationPanel() const;
    ui::navigation::NavigationPanel *mainNavigationPanel() const;
    ui::navigation::NavigationPanel *navigationPanel(const QString &surfaceId) const;
    QString activeNavigationSurfaceId() const { return m_activeSurfaceId; }

    /**
     * @brief 注册主导航子界面及主导航项快捷接口
     */
    bool addSubInterface(
        const QString &routeKey,
        QWidget *interfaceWidget,
        const QString &iconGlyph,
        const QString &text,
        const QString &parentRouteKey = QString(),
        ui::navigation::NavigationItemPosition pos = ui::navigation::NavigationItemPosition::Top,
        bool selectable = true,
        std::shared_ptr<ui::animation::AnimatedVisualSource> visualSource = nullptr
    );

    /**
     * @brief 向当前激活的导航面板添加分区分隔头
     * @param text 分区标题
     */
    void addSectionHeader(const QString &text);

    /**
     * @brief 向当前激活的导航面板添加自定义小控件
     * @param widget 控件指针
     * @param position 布局位置
     */
    void addWidget(ui::navigation::NavigationWidget *widget,
                   ui::navigation::NavigationItemPosition position = ui::navigation::NavigationItemPosition::Top);

    /**
     * @brief 设置主导航面板底部固定控件
     * @param footerWidget 底部控件指针
     */
    void setPaneFooter(ui::navigation::NavigationWidget *footerWidget);

    /**
     * @brief 获取主导航面板底部固定控件
     * @return 底部控件指针
     */
    ui::navigation::NavigationWidget *paneFooter() const;

    /**
     * @brief 获取底层 Fluent NavigationView
     * @return NavigationView 指针
     */
    fluent::navigation::NavigationView *navigationView() const { return m_navigationView; }

    bool canPresentRoute(const QString &routeKey) const override;
    bool presentRoute(const QString &routeKey, ui::navigation::NavigationDirection direction = ui::navigation::NavigationDirection::Forward) override;

Q_SIGNALS:
    /**
     * @brief 用户在导航面板等 UI 发起导航意图
     * @param routeKey 目标路由
     */
    void navigationRequested(const QString &routeKey);

    /**
     * @brief 用户触发返回（TitleBar 或 Panel Back）
     */
    void backRequested();

    /**
     * @brief 用户触发前进
     */
    void forwardRequested();

    /**
     * @brief 页面已在 UI 层面生效呈现
     * @param routeKey 当前呈现的路由
     */
    void routeChanged(const QString &routeKey);

protected:
    void initNavigation();
    void syncActivePanelState();

protected:
    fluent::navigation::NavigationView *m_navigationView = nullptr;

    QHash<QString, QPointer<ui::navigation::NavigationPanel>> m_surfaces;
    QString m_activeSurfaceId;

    QHash<QString, ui::navigation::RouteDescriptor> m_routes;
    QHash<int, QString> m_indexToRouteMap;
};
