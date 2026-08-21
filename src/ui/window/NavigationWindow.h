#pragma once

#include "ui/navigation/NavigationMetrics.h"
#include "WindowBase.h"
#include <QHash>
#include <QString>

#include <memory>

namespace ui::animation {
    class AnimatedVisualSource;
}

namespace ui::navigation {
    class NavigationPanel;
    class NavigationIndicator;
    class NavigationWidget;
    class NavigationHistory;
}

namespace fluent::navigation {
    class NavigationView;
}


class NavigationWindow : public WindowBase {
    Q_OBJECT

public:
    explicit NavigationWindow(QWidget *parent = nullptr);

    ~NavigationWindow() override;

    ui::navigation::NavigationPanel *navigationPanel() const { return m_panel; }

    /**
     * @brief 添加分区小标题
     * @param text 标题文本
     */
    void addSectionHeader(const QString &text);

    /**
     * @brief 向导航栏添加自定义部件
     * @param widget 导航部件指针
     * @param position 归属区域（Top 顶部 / Bottom 底部）
     */
    void addWidget(ui::navigation::NavigationWidget *widget,
                   ui::navigation::NavigationItemPosition position =
                           ui::navigation::NavigationItemPosition::Top);

    /**
     * @brief 注册子界面及对应的导航路由项
     * @param routeKey 唯一路由标识
     * @param interfaceWidget 页面部件指针（为 nullptr 时仅作为分类菜单）
     * @param iconGlyph 图标字形
     * @param text 显示文本
     * @param parentRouteKey 父级分类路由标识（为空时作为顶级节点）
     * @param pos 归属区域（Top / Bottom）
     * @param selectable 是否可被选中
     * @param visualSource 动态矢量动画源（可选）
     */
    void addSubInterface(
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
     * @brief 切换当前显示的子页面
     * @param routeKey 目标路由标识
     */
    void switchTo(const QString &routeKey);

    /**
     * @brief 设置底部固定槽位部件（对标 WinUI 3 PaneFooter）
     * @param footerWidget 底部槽位部件指针（继承自 NavigationWidget）
     */
    void setPaneFooter(ui::navigation::NavigationWidget *footerWidget);

    /**
     * @brief 获取当前挂载的底部槽位部件
     * @return 底部部件指针
     */
    ui::navigation::NavigationWidget *paneFooter() const;

    /**
     * @brief 获取底层导航面板实例
     * @return NavigationPanel 指针
     */
    ui::navigation::NavigationPanel *panel() const { return m_panel; }

    /**
     * @brief 获取 Fluent 原生导航视图组件
     * @return NavigationView 指针
     */
    fluent::navigation::NavigationView *navigationView() const { return m_navigationView; }

protected:
    void initNavigation();

protected:
    ui::navigation::NavigationPanel *m_panel = nullptr;
    fluent::navigation::NavigationView *m_navigationView = nullptr;
    ui::navigation::NavigationHistory *m_history = nullptr;

    QHash<QString, int> m_routeToIndexMap;
    QHash<int, QString> m_indexToRouteMap;

    bool m_isNavigatingHistory = false;
};
