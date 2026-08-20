#pragma once

#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QVector>

#include <FluentQt/Foundation.h>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "design/Elevation.h"

#include "ui/navigation/NavigationFocusHost.h"
#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationTreeItem.h"
#include "ui/navigation/NavigationTreeWidgetBase.h"
#include "ui/navigation/NavigationWidget.h"

class QBoxLayout;
class QPropertyAnimation;
class QParallelAnimationGroup;
class QHideEvent;
class QPaintEvent;

namespace ui::navigation {
    class NavigationTreeWidget;

    /**
     * @brief 导航菜单专用 Flyout
     *
     * 以独立 Qt::Tool 顶层窗口承载导航 flyout，可越过主窗口边界渲染。作为
     * NavigationTreeWidgetBase 的派生，内部以克隆子树渲染被激活分类的子项（可折叠、带选中态），
     * 叠加 Medium 阴影、滑入动画、屏幕边界 clamp 与上下翻转，以及锚点跟随与 light dismiss。
     */
    class NavigationFlyout : public NavigationTreeWidgetBase, public INavigationFocusHost {
        Q_OBJECT
        Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen)

        Q_PROPERTY(Placement placement READ placement WRITE setPlacement)

        Q_PROPERTY(int anchorOffset READ anchorOffset WRITE setAnchorOffset)

    public:
        enum class Placement {
            Right, // Compact 侧边栏模式：向右弹出
            Bottom, // Top 顶栏模式：向下弹出 (默认左对齐或居中)
            BottomRight, // Top 顶栏模式：向下弹出，右对齐 (常用于 Overflow 菜单)
            Auto, // 自动检测屏幕边缘并翻转
        };

        Q_ENUM(Placement)

        enum CloseFlag {
            NoAutoClose = 0,
            CloseOnPressOutside = 1 << 0,
            CloseOnEscape = 1 << 1,
        };

        Q_DECLARE_FLAGS(ClosePolicy, CloseFlag)

        /**
         * @brief 构造独立 flyout 窗口。
         * @param rootTree 原 panel 的导航树（单一权威状态源，切页/选中态统一由其驱动）
         * @param host 宿主（NavigationPanel），用于主题继承 + 锚点跟随 + eventFilter 挂载
         */
        explicit NavigationFlyout(NavigationTreeWidget *rootTree, QWidget *host);

        ~NavigationFlyout() override;

        // 锚点与定位
        QWidget *anchor() const { return m_anchorWidget; }
        void setAnchor(QWidget *anchor) { m_anchorWidget = anchor; }

        Placement placement() const { return m_placement; }
        void setPlacement(Placement p) { m_placement = p; }

        int anchorOffset() const { return m_anchorOffset; }
        void setAnchorOffset(int px) { m_anchorOffset = px; }

        // 状态与策略
        bool isOpen() const { return m_isOpen; }

        void setIsOpen(bool open);

        ClosePolicy closePolicy() const { return m_closePolicy; }
        void setClosePolicy(ClosePolicy policy) { m_closePolicy = policy; }

        bool isExitAnimationEnabled() const { return m_exitAnimationEnabled; }
        void setExitAnimationEnabled(bool enabled) { m_exitAnimationEnabled = enabled; }

        /**
         * @brief 设置点击外部时是否吞噬事件（默认为 true）。
         */
        bool lightDismissConsumesPress() const { return m_lightDismissConsumesPress; }
        void setLightDismissConsumesPress(bool consume) { m_lightDismissConsumesPress = consume; }

        /**
         * @brief 穿透白名单。
         * 点击这些控件时，弹窗关闭且事件会直接穿透，不会被吞噬。
         */
        void addLightDismissPassthrough(QWidget *widget) { m_lightDismissPassthrough.append(widget); }
        void removeLightDismissPassthrough(QWidget *widget) { m_lightDismissPassthrough.removeOne(widget); }
        void clearLightDismissPassthrough() { m_lightDismissPassthrough.clear(); }

        // 业务数据装载
        /**
         * @brief 以克隆方式重建指定分类的子树并填充 flyout。
         * @param categoryKey 被激活分类的路由键（其子项及递归子项将被克隆）
         */
        void rebuildSubtree(const QString &categoryKey);

        /**
         * @brief 以克隆方式重建给定溢出条目集合（供 overflow flyout 使用）。
         * @param entries 溢出条目列表（节点连同其递归子项将被克隆，分区标题按序渲染）
         */
        void rebuildSubtreeFromEntries(const QVector<NavigationOverflowEntry> &entries);

        NavigationIndicator *indicator() const { return m_flyoutIndicator; }
        QWidget *host() const { return m_host; }

        /**
         * @brief 触发浮层内选中项跨窗口飞跃入场（Cross Window Portal In）动画。
         */
        void playSelectedItemCrossPortal(NavigationTreeItem *selectedItem, const QRectF &mappedStartRect,
                                         const QRectF &targetRect);

        /// 获取内部克隆替身
        NavigationTreeItem *cloneItemFor(const QString &routeKey) const {
            return m_itemIndex.value(routeKey, nullptr);
        }

        NavigationTreeItem *getVisualProxyFor(NavigationTreeItem *item) const;

        QRectF indicatorRectInHost(NavigationTreeItem *item) const;

        void playInternalFlight(const QRectF &targetRect);

    public
        slots:
        /// 在指定锚点弹出（按 placement 计算位置 + 边界检查 + 入场动画）




        void showAt(QWidget *anchor);

        /// 配合外部计算好偏移后的手动打开
        void openAt(const QPoint &globalCardTopLeft, const QPoint &slideInOffset = QPoint(0, 0));

        /// 仅用于状态绑定
        void open();

        /// 关闭浮层（按 exitAnimationEnabled 决定走 120ms 退场还是瞬退）
        void close();

        void onIndicatorOwnerChanged(NavigationTreeItem *item, bool isOwner);

        void moveFocusBy(int delta) override;

        signals:
        /// 即将开始展示（入场动效开始前）




        void aboutToShow();

        /// 已完全打开（入场动效结束）
        void opened();

        /// 即将开始关闭（退场动效开始瞬间，供顶栏指示条并发归位）
        void aboutToHide();

        /// 已完全关闭（退场动效结束，供销毁）
        void closed();

        void expansionChanged(const QString &routeKey, bool expanded);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

        bool event(QEvent *event) override;

        void showEvent(QShowEvent *event) override;

        void hideEvent(QHideEvent *event) override;

        void paintEvent(QPaintEvent *event) override;

        void keyPressEvent(QKeyEvent *event) override;

        void onThemeUpdated() override { update(); }

    private:
        QVector<NavigationWidget *> visibleItems() const;

        void collectVisible(NavigationTreeWidget *node, QVector<NavigationWidget *> &out) const;

        void cloneNode(NavigationTreeWidget *srcNode, NavigationTreeWidget *parentClone, int depth);

        void finalizeSize();

        QWidget *m_host = nullptr;
        QBoxLayout *m_contentLayout = nullptr;

        QPoint m_globalCardPos;
        QPoint m_slideInOffset;
        QPoint m_hostAnchorOffset;
        bool m_flippedUp = false;

        QPointer<QWidget> m_anchorWidget;
        Placement m_placement = Placement::Bottom;
        int m_anchorOffset = 0;

        bool m_isOpen = false;
        ClosePolicy m_closePolicy = ClosePolicy(CloseOnPressOutside | CloseOnEscape);
        bool m_exitAnimationEnabled = true;

        bool m_lightDismissConsumesPress = true;
        QVector<QPointer<QWidget> > m_lightDismissPassthrough;

        QParallelAnimationGroup *m_animGroup = nullptr;
        QPropertyAnimation *m_slideAnim = nullptr;
        QPropertyAnimation *m_fadeAnim = nullptr;

        NavigationIndicator *m_flyoutIndicator = nullptr;
        QHash<QString, NavigationTreeItem *> m_itemIndex;
        bool m_isClosing = false;
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS (NavigationFlyout::ClosePolicy)
} // namespace ui::navigation
