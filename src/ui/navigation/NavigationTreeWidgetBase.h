#pragma once

#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

#include <FluentQt/Foundation.h>

class QVBoxLayout;
class QVariantAnimation;

namespace ui::navigation {
    class NavigationWidget;
    class NavigationTreeItem;
    class NavigationTreeWidget;

    /**
 * @brief 导航树节点容器基类
 *
 * 抽象节点容器的「结构 + 视觉条目 + 展开」职责：每个节点持有视觉条目（itemWidget）、
 * 子节点列表与折叠子容器。根资源（滚动视图、指示条、溢出、全局索引）由派生类
 * NavigationTreeWidget 承载，节点通过 m_root 反查根做全局操作（选中/指示条/弹 flyout）。
 */
    class NavigationTreeWidgetBase : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        ~NavigationTreeWidgetBase() override;

        /// 分类态由子项推导：有子项即分类，无子项即叶子
        bool isCategory() const { return !m_children.isEmpty(); }

        /// 视觉条目强类型访问
        NavigationTreeItem *item();

        const NavigationTreeItem *item() const;

        QString routeKey() const { return m_routeKey; }
        NavigationWidget *itemWidget() const { return m_itemWidget; }
        const QVector<NavigationTreeWidget *> &children() const { return m_children; }

        NavigationTreeWidget *parentNode() const;

        /// 折叠/展开自身：手风琴语义，同步 itemWidget 箭头与子容器高度
        void setExpanded(bool expanded, bool animated = true);

        /// 沿父链展开所有祖先分类，使目标节点可见
        void expandAncestors(NavigationTreeWidget *node);

        /// 决策槽：itemWidget 的 itemClicked 统一入口，依据分类/可选中/模式分派
    protected:
        void onItemClicked(const QString &routeKey, bool chevronClicked);

    public:
        signals:
        /// 展开态变化，供根统一处理指示条归属等副作用




        void expansionChanged(const QString &routeKey, bool expanded);

    protected:
        explicit NavigationTreeWidgetBase(NavigationTreeWidget *root, QWidget *parent = nullptr);

        NavigationTreeWidget *root();

        const NavigationTreeWidget *root() const;

        /// 折叠子容器高度动画：逐节点独立，以归一化进度驱动 setFixedHeight
        void animateChildrenContainer(bool expanded);

        // 节点容器成员
        /// 根实例：根自身指向自己，QPointer 防悬空
        QPointer<NavigationTreeWidget> m_root;
        /// 视觉条目：叶子可为任意 NavigationWidget
        NavigationWidget *m_itemWidget = nullptr;
        QString m_routeKey;
        QPointer<NavigationTreeWidget> m_parentNode;
        QVector<NavigationTreeWidget *> m_children;
        QWidget *m_childrenContainer = nullptr;
        QVBoxLayout *m_childrenLayout = nullptr;
        bool m_isExpanded = false;
        bool m_rememberExpandState = false;
        bool m_savedExpanded = false;
        /// 每个节点独立的折叠高度动画
        QVariantAnimation *m_heightAnimation = nullptr;
    };
} // namespace ui::navigation
