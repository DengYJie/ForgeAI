#include "ui/navigation/NavigationTreeWidgetBase.h"

#include <QEasingCurve>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <FluentQt/Design.h>

#include "ui/navigation/NavigationTreeItem.h"
#include "ui/navigation/NavigationTreeWidget.h"
#include "ui/navigation/NavigationWidget.h"

namespace ui::navigation {
    NavigationTreeWidgetBase::NavigationTreeWidgetBase(NavigationTreeWidget *root, QWidget *parent)
        : QWidget(parent)
          , m_root(root) {
    }

    NavigationTreeWidgetBase::~NavigationTreeWidgetBase() = default;

    NavigationTreeWidget *NavigationTreeWidgetBase::parentNode() const {
        return m_parentNode.data();
    }

    NavigationTreeWidget *NavigationTreeWidgetBase::root() {
        return m_root.data();
    }

    const NavigationTreeWidget *NavigationTreeWidgetBase::root() const {
        return m_root.data();
    }

    NavigationTreeItem *NavigationTreeWidgetBase::item() {
        return static_cast<NavigationTreeItem *>(m_itemWidget);
    }

    const NavigationTreeItem *NavigationTreeWidgetBase::item() const {
        return static_cast<const NavigationTreeItem *>(m_itemWidget);
    }

    void NavigationTreeWidgetBase::animateChildrenContainer(bool expanded) {
        if (!m_childrenContainer)
            return;

        if (NavigationWidget::isReducedMotion()) {
            m_childrenContainer->setVisible(expanded);
            return;
        }

        QWidget *container = m_childrenContainer;
        if (!m_heightAnimation) {
            m_heightAnimation = new QVariantAnimation(this);
            connect(m_heightAnimation, &QVariantAnimation::valueChanged, this, [container](const QVariant &v) {
                container->setFixedHeight(v.toInt());
            });
        }

        auto *anim = m_heightAnimation;
        anim->stop();
        anim->disconnect(SIGNAL(finished()));

        anim->setDuration(themeAnimation().fast);
        anim->setEasingCurve(themeAnimation().decelerate);

        if (expanded) {
            const int target = container->layout()
                                   ? container->layout()->sizeHint().height()
                                   : container->sizeHint().height();
            const int current = container->isVisible() ? container->height() : 0;

            container->show();
            anim->setStartValue(current);
            anim->setEndValue(target);
            connect(anim, &QVariantAnimation::finished, this, [this, container]() {
                container->setMinimumHeight(0);
                container->setMaximumHeight(QWIDGETSIZE_MAX);
                emit expansionChanged(m_routeKey, true);
            }, Qt::SingleShotConnection);
            anim->start();
        } else {
            const int current = container->height();

            anim->setStartValue(current);
            anim->setEndValue(0);
            connect(anim, &QVariantAnimation::finished, this, [container]() {
                container->setFixedHeight(0);
                container->hide();
                container->setMinimumHeight(0);
                container->setMaximumHeight(QWIDGETSIZE_MAX);
            }, Qt::SingleShotConnection);
            anim->start();

            // 折叠时即刻发射：父分类坐标即刻生效，指示条立即并发滑向父分类替身
            emit expansionChanged(m_routeKey, false);
        }
    }

    void NavigationTreeWidgetBase::setExpanded(bool expanded, bool animated) {
        if (!isCategory() || m_isExpanded == expanded)
            return;
        m_isExpanded = expanded;

        item()->setExpanded(expanded, animated);

        if (m_childrenContainer) {
            if (!animated || NavigationWidget::isReducedMotion()) {
                m_childrenContainer->setVisible(expanded);
                emit expansionChanged(m_routeKey, expanded);
            } else {
                animateChildrenContainer(expanded);
            }
        } else {
            emit expansionChanged(m_routeKey, expanded);
        }
    }

    void NavigationTreeWidgetBase::expandAncestors(NavigationTreeWidget *node) {
        QVector<NavigationTreeWidget *> chain;
        for (NavigationTreeWidget *p = node->m_parentNode; p; p = p->m_parentNode)
            chain.prepend(p);
        for (NavigationTreeWidget *a: chain)
            a->setExpanded(true, false);
    }

    void NavigationTreeWidgetBase::onItemClicked(const QString &routeKey, bool chevronClicked) {
        auto *rootNode = root();

        // 叶子项：主体/chevron 点击均只做选中切页
        if (!isCategory()) {
            rootNode->setCurrentItem(routeKey);
            return;
        }

        // flyout 内克隆的分类节点：始终内联展开，不参与 flyout 模式（否则会二次弹 flyout）
        const auto *selfTree = qobject_cast<const NavigationTreeWidget *>(this);
        const bool inlineExpansion = selfTree && selfTree->inlineExpansion();

        const bool flyoutMode = !inlineExpansion
                                && (rootNode->isCompacted()
                                    || rootNode->orientation() == Qt::Horizontal);

        if (flyoutMode) {
            // 紧凑/Top：分类通过 flyout 渲染子项，chevron 点击做激活↔反激活切换
            if (chevronClicked && rootNode->isCategoryActive(routeKey)) {
                // 已激活 → 反激活（关闭 flyout）
                rootNode->dismissCategory();
            } else {
                // 激活：箭头随状态转动（激活=180°），flyout 弹出
                item()->animateChevron(180.0f);
                rootNode->activateCategory(routeKey, m_itemWidget);

                if (!chevronClicked && m_itemWidget && m_itemWidget->isSelectable())
                    rootNode->setCurrentItem(routeKey);
            }

            return;
        }

        setExpanded(!m_isExpanded, true);

        // Left 非紧凑：chevron 只展开/收起，主体点击展开后按可选中性切页
        if (chevronClicked) return;

        if (m_itemWidget &&m_itemWidget->isSelectable())
        // flyout 内（inlineExpansion）点击 selectable 分类项：仅切选中态，不立即重算 overflow，
        // 待 flyout 关闭后由 dismissCategory 触发重排，避免 flyout 打开期间顶栏布局抖动
        rootNode->setCurrentItem(routeKey, !inlineExpansion);
    }
} // namespace ui::navigation
