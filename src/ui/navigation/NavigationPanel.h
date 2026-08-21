#pragma once

#include <QPointer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QWidget>

#include <QRect>

#include <FluentQt/Foundation.h>

#include "ui/navigation/NavigationFlyout.h"
#include "ui/navigation/NavigationFocusHost.h"
#include "ui/navigation/NavigationMetrics.h"

#include <memory>

class QPaintEvent;
class QShowEvent;

namespace ui::animation {
    class AnimatedIcon;
    class AnimatedVisualSource;
}

namespace ui::navigation {
    class NavigationIndicator;
    class NavigationTreeItem;
    class NavigationTreeWidget;
    class NavigationToolButton;
    class NavigationWidget;

    /**
     * @brief 导航整合面板
     *
     * 作为 NavigationView 的 mainChromeWidget，集中承载顶部操作区（返回与汉堡按钮）、主导航树（Main）、
     * 底部操作区（Footer）及用户卡片。通过 resizeEvent 实时监听物理宽度推算折叠进度并派发至子树，
     * 保证指示条与布局在 Auto 断点切换时的精确同步。
     */
    class NavigationPanel : public QWidget, public fluent::FluentElement,
                            public INavigationFocusHost {
        Q_OBJECT
        Q_PROPERTY(bool backButtonVisible READ isBackButtonVisible WRITE setBackButtonVisible)

        Q_PROPERTY(bool backEnabled READ isBackEnabled WRITE setBackEnabled)

        Q_PROPERTY(bool paneToggleButtonVisible READ isPaneToggleButtonVisible WRITE setPaneToggleButtonVisible)

        Q_PROPERTY(bool compacted READ isCompacted WRITE setCompacted NOTIFY compactedChanged)

        Q_PROPERTY(bool selectionFollowsFocus READ selectionFollowsFocus WRITE setSelectionFollowsFocus)

    public:
        explicit NavigationPanel(QWidget *parent = nullptr);

        ~NavigationPanel() override = default;

        /**
         * @brief 添加导航路由项
         * @param routeKey 唯一路由标识
         * @param iconGlyph Fluent 图标字形
         * @param text 显示文本
         * @param parentKey 父级分类标识，为空时作为顶级节点
         * @param position 条目归属区域（Top 主列表 / Bottom 页脚），默认 Top
         * @param selectable 是否可选中，默认 true
         * @param tooltip 提示文本（为空时默认使用 text）
         * @param visualSource 动态矢量动画源（可选）
         */
        void addItem(const QString &routeKey, const QString &iconGlyph,
                     const QString &text, const QString &parentKey = QString(),
                     NavigationItemPosition position = NavigationItemPosition::Top,
                     bool selectable = true,
                     const QString &tooltip = QString(),
                     std::shared_ptr<ui::animation::AnimatedVisualSource> visualSource = nullptr);

        /**
         * @brief 添加分区标题
         * @param text 分区文本
         */
        void addSectionHeader(const QString &text);

        /**
         * @brief 添加自定义导航部件（透传到树）
         * @param widget 已构造的导航部件
         * @param position 条目归属区域（Top 主列表 / Bottom 页脚）
         */
        void addWidget(NavigationWidget *widget,
                       NavigationItemPosition position = NavigationItemPosition::Top);

        /**
         * @brief 设置当前激活路由
         * @param routeKey 目标路由标识
         */
        void setCurrentItem(const QString &routeKey);

        /**
         * @brief 获取当前激活路由标识
         * @return 当前路由标识字符串
         */
        QString currentRouteKey() const;

        /**
         * @brief 设置底部固定槽位部件（对标 WinUI 3 PaneFooter）
         * @param footerWidget 底部槽位部件指针（继承自 NavigationWidget，可感知 Compact/Orientation）
         */
        void setPaneFooter(NavigationWidget *footerWidget);

        /**
         * @brief 获取当前挂载的底部槽位部件
         * @return 底部槽位部件指针
         */
        NavigationWidget *paneFooter() const { return m_paneFooter; }

        /**
         * @brief 编程式展开/收起指定分类（紧凑模式下忽略）
         * @param routeKey 分类节点路由键
         * @param expanded 是否展开
         * @param animated 是否启用折叠高度过渡动画
         */
        void setCategoryExpanded(const QString &routeKey, bool expanded, bool animated = true);

        /**
         * @brief 获取导航选中指示条实例
         * @return 指示条指针
         */
        NavigationIndicator *indicator() const { return m_indicator; }

        /**
         * @brief 获取导航树部件实例
         * @return 导航树指针
         */
        NavigationTreeWidget *tree() const { return m_tree; }

        /**
         * @brief 获取返回按钮实例
         * @return 返回按钮指针
         */
        NavigationToolButton *backButton() const { return m_backButton; }

        /**
         * @brief 获取侧边栏展开/折叠汉堡按钮实例
         * @return 汉堡按钮指针
         */
        NavigationToolButton *paneToggleButton() const { return m_paneToggleButton; }

        /**
         * @brief 判断返回按钮是否可见
         * @return true 为可见
         */
        bool isBackButtonVisible() const;

        /**
         * @brief 设置返回按钮可见性
         * @param visible 是否可见
         */
        void setBackButtonVisible(bool visible);

        /**
         * @brief 判断返回按钮是否可用
         * @return true 为可用
         */
        bool isBackEnabled() const;

        /**
         * @brief 设置返回按钮可用性
         * @param enabled 是否可用
         */
        void setBackEnabled(bool enabled);

        /**
         * @brief 判断汉堡按钮是否可见
         * @return true 为可见
         */
        bool isPaneToggleButtonVisible() const;

        /**
         * @brief 设置汉堡按钮可见性
         * @param visible 是否可见
         */
        void setPaneToggleButtonVisible(bool visible);

        /**
         * @brief 判断汉堡按钮是否被显式隐藏
         * @return true 为被显式隐藏
         */
        bool isPaneToggleExplicitlyHidden() const { return m_paneToggleExplicitlyHidden; }

        /**
         * @brief 设置汉堡按钮是否被显式隐藏
         * @param hidden 是否隐藏
         */
        void setPaneToggleExplicitlyHidden(bool hidden);

        /**
         * @brief 判断背景表面是否可见（抽屉浮层模式下透出毛玻璃）
         * @return true 为表面可见
         */
        bool isSurfaceVisible() const { return m_surfaceVisible; }

        /**
         * @brief 判断当前是否处于紧凑图标列模式 (48px)
         * @return true 为紧凑模式
         */
        bool isCompacted() const { return m_isCompacted; }

        /**
         * @brief 设置是否处于紧凑图标列模式
         * @param compacted 是否紧凑
         */
        void setCompacted(bool compacted);

        /**
         * @brief 判断是否启用“焦点跟随选中”模式
         * @return true 为焦点跟随
         */
        bool selectionFollowsFocus() const;

        /**
         * @brief 设置是否启用“焦点跟随选中”模式
         * @param follows 是否跟随
         */
        void setSelectionFollowsFocus(bool follows);

        /**
         * @brief 刷新指示条几何位置（公共接口，供树节点几何突变时重绘调用）
         * @param animated 是否启用几何平滑插值动画
         */
        void updateIndicatorVisuals(bool animated = false) { refreshIndicatorVisuals(animated); }

        /**
         * @brief 获取指定路由对应的可视化部件或浮层代理部件
         * @param routeKey 路由键
         * @return 对应的部件指针
         */
        QWidget *visualWidgetForRoute(const QString &routeKey) const;

        /**
         * @brief 切换侧边栏展开/收起状态
         */
        void togglePane();

        /**
         * @brief 设置导航栏排版方向（水平/垂直）
         * @param orientation 排版方向
         */
        void setOrientation(Qt::Orientation orientation);

        /**
         * @brief 获取当前排版方向
         * @return 排版方向枚举
         */
        Qt::Orientation orientation() const { return m_orientation; }

        /**
         * @brief 向上或向下移动焦点
         * @param delta 步长，-1 为上/左，+1 为下/右
         */
        void moveFocusBy(int delta) override;

        /**
         * @brief 获取部件建议尺寸
         * @return 建议尺寸
         */
        QSize sizeHint() const override;

        signals:
        /// 切页的唯一驱动源，由树统一广播




        void itemSelected(const QString &routeKey);

        void backRequested();

        void compactedChanged(bool isCompacted);

        void indicatorOwnerChanged(NavigationTreeItem *item, bool isOwner);

    public:
        void onThemeUpdated() override { update(); }

    protected:
        void paintEvent(QPaintEvent *event) override;

        void showEvent(QShowEvent *event) override;

        bool event(QEvent *event) override;

    private:
        void setupUi();

        void setSurfaceVisible(bool visible);

        void showFlyoutMenu(const QString &categoryKey, QWidget *anchorWidget);

        void showOverflowMenu(QWidget *anchorWidget, const QVector<NavigationOverflowEntry> &entries);

        void closeFlyoutMenu(bool animated = true);

        void closeOverflowMenu(bool animated = true);

        void triggerCrossWindowPortal(NavigationFlyout *flyout, QWidget *anchorWidget, NavigationTreeItem *prevOwner,
                                      NavigationTreeItem *curClone = nullptr);

        void dispatchIndicatorAnimation(NavigationTreeItem *prevOwner, NavigationTreeItem *curOwner);

        void animateFlyoutClosed(const QString &categoryKey, QWidget *anchorWidget);

        void refreshIndicatorVisuals(bool animated = false, NavigationTreeItem *prevOwner = nullptr);

        /// Flyout 关闭后应执行的动画意图
        enum class FlyoutCloseIntent {
            None, // 无特殊动画（light-dismiss、owner 不属于此 flyout）
            LeafSlide, // 叶子点击：顶栏指示条从 prevOwner 滑向 curOwner 的视觉代理
            PortalReturn, // 指示条正在 flyout 内部：播放 Portal 归位动画
        };

    private:
        bool m_isCompacted = false;
        bool m_surfaceVisible = false;
        bool m_paneToggleExplicitlyHidden = false;
        Qt::Orientation m_orientation = Qt::Vertical;

        QBoxLayout *m_layout = nullptr;
        QBoxLayout *m_headerLayout = nullptr;
        NavigationToolButton *m_backButton = nullptr;
        ui::animation::AnimatedIcon *m_animatedBackIcon = nullptr;
        NavigationToolButton *m_paneToggleButton = nullptr;
        ui::animation::AnimatedIcon *m_animatedPaneToggleIcon = nullptr;
        NavigationTreeWidget *m_tree = nullptr;
        NavigationIndicator *m_indicator = nullptr;
        QWidget *m_paneFooterContainer = nullptr;
        QBoxLayout *m_paneFooterLayout = nullptr;
        QPointer<NavigationWidget> m_paneFooter;
        QPointer<NavigationTreeItem> m_indicatorOwner;
        QPointer<NavigationTreeItem> m_visualIndicatorOwner;

        QPointer<NavigationFlyout> m_activeFlyout;
        FlyoutCloseIntent m_flyoutCloseIntent = FlyoutCloseIntent::None;
        QPointer<NavigationTreeItem> m_flyoutPrevOwner;
    };
} // namespace ui::navigation
