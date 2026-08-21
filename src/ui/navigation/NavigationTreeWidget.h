#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

#include <FluentQt/Foundation.h>

#include "ui/navigation/NavigationFocusHost.h"
#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationTreeWidgetBase.h"

#include <QPointer>
#include <memory>

class QBoxLayout;
class QPaintEvent;
class QResizeEvent;

namespace fluent::scrolling {
    class ScrollView;
}

namespace ui::animation {
    class AnimatedVisualSource;
}

namespace ui::navigation {
    class NavigationWidget;
    class NavigationTreeItem;
    class NavigationToolButton;
    class NavigationIndicator;
    class NavigationSectionHeader;

    /**
     * @brief 导航树根容器
     *
     * 继承 NavigationTreeWidgetBase（节点容器），同时承载根资源：滚动视图、指示条、
     * 溢出控制器与全局路由索引。每个节点（含根自身）都是本类实例，节点通过
     * m_root 反查根做全局操作（选中/指示条/弹 flyout）。
     */
    class NavigationTreeWidget : public NavigationTreeWidgetBase, public INavigationFocusHost {
        Q_OBJECT
        Q_PROPERTY(bool selectionFollowsFocus READ selectionFollowsFocus WRITE setSelectionFollowsFocus)

        // flyout 通过克隆节点重建子树，需访问节点受保护的结构成员
        friend class NavigationFlyout;
        friend class NavigationPanel;
        friend class NavigationTreeWidgetBase;

    public:
        explicit NavigationTreeWidget(QWidget *parent = nullptr);

        ~NavigationTreeWidget() override = default;

    private:
        /// 节点模式构造：挂载到 rootNode，不初始化根资源
        explicit NavigationTreeWidget(NavigationTreeWidget *rootNode);

    public:
        /**
         * @brief 注册节点
         * @param routeKey 唯一路由键
         * @param iconGlyph Fluent 图标字形
         * @param text 显示文本
         * @param parentKey 父级分类键（为空时作为顶级节点）
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
         * @param text 分区标题文本
         */
        void addSectionHeader(const QString &text);

        /**
         * @brief 添加自定义导航部件
         *
         * 作为条目落地的统一入口：负责方向、展开进度同步与布局插入，并登记分区标题。
         * 自定义部件可在传入前自行连接 clicked 信号；addItem 与 addSectionHeader 均收敛于此。
         * @param widget 已构造的导航部件（NavigationWidget 子类，可为 NavigationSectionHeader）
         * @param position 条目归属区域（Top 主列表 / Bottom 页脚）
         */
        void addWidget(NavigationWidget *widget,
                       NavigationItemPosition position = NavigationItemPosition::Top);

        /**
         * @brief 从树中移除指定节点（含其所有子孙节点）。
         * @param routeKey 目标路由键
         * @return 节点存在且被移除返回 true
         */
        bool removeItem(const QString &routeKey);

        NavigationTreeWidget *nodeFor(const QString &routeKey);

        const NavigationTreeWidget *nodeFor(const QString &routeKey) const;

        /**
         * @brief 激活指定路由节点并定位指示条
         * @param routeKey 目标路由键
         * @param updateOverflow 是否立即重算 overflow 布局（Top 模式）；false 时仅更新选中态，
         *                       由调用方在合适的时机（如 flyout 关闭）再触发重算
         */
        void setCurrentItem(const QString &routeKey, bool updateOverflow = true);

        bool contains(const QString &routeKey) const;

        /**
         * @brief 判定节点是否为根节点（无父分类）。
         * @param routeKey 目标路由键
         */
        bool isRoot(const QString &routeKey) const;

        /**
         * @brief 判定节点是否为叶子节点（无子项）。
         * @param routeKey 目标路由键
         */
        bool isLeaf(const QString &routeKey) const;

        QString currentRouteKey() const { return m_currentRouteKey; }

    private:
        NavigationTreeItem *getVisualProxyFor(NavigationTreeItem *logicalOwner) const;

    public:
        NavigationTreeItem *findFirstSelectableItem() const;

        /**
         * @brief 设置紧凑图标栏模式
         *
         * 紧凑模式下，点击分类不再内联展开子项，而是由宿主（NavigationPanel）弹 flyout 显示子项。
         */
        void setCompacted(bool compacted);

        bool isCompacted() const { return m_isCompacted; }

        /**
         * @brief 设置面板布局方向（Horizontal / Vertical）
         */
        void setOrientation(Qt::Orientation orientation);

        Qt::Orientation orientation() const { return m_orientation; }

    private:
        /// 标记节点为 flyout 内克隆节点：点击分类时内联展开而非再弹 flyout
        void setInlineExpansion(bool inlineMode) { m_inlineExpansion = inlineMode; }
        bool inlineExpansion() const { return m_inlineExpansion; }

    private
        slots:




        void onIndicatorOwnerChanged(NavigationTreeItem *item, bool isOwner);

    private:
        /// 计算给定控件在其宿主坐标系下的指示条目标矩形
        QRectF indicatorRectInHost(NavigationWidget *item, QWidget *host = nullptr) const;

    public:
        /**
         * @brief 判定节点是否为目标节点的后代
         */
        bool isAncestorOf(const QString &routeKey, const QString &ancestorKey) const;

        /**
         * @brief 编程式展开/收起指定分类（紧凑模式下忽略，交由 flyout 处理）。
         * @param routeKey 分类节点路由键
         * @param expanded 是否展开
         * @param animated 是否启用折叠高度过渡动画
         */
        void setCategoryExpanded(const QString &routeKey, bool expanded, bool animated = true);

        /**
         * @brief 设置分类节点是否记忆其展开状态。
         * @param routeKey 分类节点路由键
         * @param remember 是否记忆
         */
        void setRememberExpandState(const QString &routeKey, bool remember);

    private:
        /**
         * @brief 保存分类节点当前展开状态（供后续恢复）。
         */
        void saveExpandState(const QString &routeKey);

        /**
         * @brief 恢复分类节点保存的展开状态。
         */
        void restoreExpandState(const QString &routeKey, bool animated = true);

        void collapseCategoryChevron(const QString &categoryKey);

        /**
         * @brief 通知树：某分类的 flyout 因外部原因（点外部/Esc/选子项）被关闭。
         * 树据此清空激活态并回收 chevron，保持状态单一权威。
         */
        void dismissCategory();

        /**
         * @brief 重算 overflow（Top 模式）。
         *
         * flyout 内点击 selectable 分类项时延迟了 overflow 重算，此方法在 flyout 关闭后
         * 补齐延迟的溢出重排与指示条更新。
         */
        void updateOverflow();

        /**
         * @brief 激活分类（设置激活态 + 发 categoryActivated），由节点点击决策调用。
         * 若已是当前激活分类则忽略（幂等）。
         */
        void activateCategory(const QString &categoryKey, QWidget *anchorWidget);

        /// 判定某分类是否处于 flyout 激活态
        bool isCategoryActive(const QString &categoryKey) const { return m_activeCategoryKey == categoryKey; }

    public:
        void moveFocusBy(int delta) override;

        bool selectionFollowsFocus() const { return m_selectionFollowsFocus; }

        void setSelectionFollowsFocus(bool follows);

        /// 是否有溢出项（Top 模式栏内容超宽时）
        bool hasOverflowItems() const { return !m_overflowNodes.isEmpty() || !m_overflowHeaders.isEmpty(); }

        signals:
        /// 切页的唯一驱动源：所有交互路径收敛到 setCurrentItem 后发出，避免多头触发




        void itemSelected(const QString &routeKey);

        /// 紧凑模式下点击分类项时发出（携带分类项对应的按钮组件，供 flyout 锚定与防抖）
        void categoryActivated(const QString &categoryKey, QWidget *anchorWidget);

        /// 分类 flyout 关闭（反激活）时发出，与 categoryActivated 对称
        void categoryDeactivated(const QString &categoryKey);

        /// 溢出菜单被请求（用户点击溢出按钮），携带锚点组件与溢出条目快照，供 panel 渲染
        void overflowMenuRequested(QWidget *anchorWidget, const QVector<NavigationOverflowEntry> &entries);

    public:
        void onThemeUpdated() override;

        /**
         * @brief 确保内置 ScrollView 及其视口保持透明底色，避免覆盖面板材质
         */
        void applyTransparentScrollView();

    protected:
        void paintEvent(QPaintEvent *event) override;

        void showEvent(QShowEvent *event) override;

        void resizeEvent(QResizeEvent *event) override;

    private:
        /// 展开态变化后统一调整指示条归属（折叠回退父级，展开交还子项）
        void onExpansionChanged(const QString &routeKey, bool expanded);

        QVector<NavigationWidget *> visibleItems() const;

        void collectVisible(NavigationTreeWidget *node, QVector<NavigationWidget *> &out) const;

        /// 折叠终态：捕获所有分类展开态快照并收起子容器
        void collapseAllCategories();

        /// 展开终态：按折叠前快照恢复所有分类展开态
        void restoreAllCategories();

        void propagateOrientation(Qt::Orientation orientation);

        bool isSelectedUnder(NavigationTreeWidget *node) const;

        // 溢出计算（Top 模式）
        /// 重算溢出排布；返回是否发生了可见性/布局变动（供调用方决定指示条是否需要动画）
        bool computeOverflow(bool animated = false);

        /// 按布局顺序收集溢出条目（供溢出信号携带快照）
        QVector<NavigationOverflowEntry> overflowEntries() const;

        QHash<QString, NavigationTreeWidget *> m_nodeIndex;

        QBoxLayout *m_layout = nullptr;
        QBoxLayout *m_mainLayout = nullptr;
        QBoxLayout *m_footerLayout = nullptr;
        bool m_addingFooter = false;
        QVector<NavigationSectionHeader *> m_headers;
        QString m_currentRouteKey;
        /// 当前 flyout 激活的分类（空 = 无），激活态单一权威
        QString m_activeCategoryKey;
        bool m_isCompacted = false;
        /// 折叠终态捕获的所有分类展开态快照（routeKey -> 是否展开），展开终态据此恢复
        QHash<QString, bool> m_savedExpandStates;
        /// 是否为 flyout 内克隆节点：分类点击走内联展开而非 flyout 模式
        bool m_inlineExpansion = false;
        Qt::Orientation m_orientation = Qt::Vertical;
        fluent::scrolling::ScrollView *m_scrollView = nullptr;

        // 溢出状态（Top 模式）
        NavigationToolButton *m_overflowButton = nullptr;
        QVector<NavigationTreeWidget *> m_overflowNodes;
        QVector<NavigationSectionHeader *> m_overflowHeaders;
        /// 粘性保留在栏内的顶级分类 Key（仅在缩小窗口或在 overflow 中选择新项时才取消/更换）
        QString m_pinnedCategoryKey;
        /// 上次可用宽度，用于检测窗口是否在缩小（缩小即取消粘性保护）
        int m_lastTopAvailableWidth = 0;
        /// 防止 layout 迭代期间重入
        bool m_updatingOverflow = false;
        /// 防止 setCurrentItem 信号级联重入
        bool m_settingCurrentItem = false;
        bool m_selectionFollowsFocus = false;
    };
} // namespace ui::navigation
