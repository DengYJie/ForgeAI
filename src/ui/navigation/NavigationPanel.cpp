#include "ui/navigation/NavigationPanel.h"

#include <QApplication>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QShowEvent>
#include <QTimer>

#include <FluentQt/Design.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/Windowing.h>

#include "ui/animation/AnimatedIcon.h"
#include "ui/animation/AnimatedBackVisualSource.h"
#include "ui/animation/AnimatedGlobalNavVisualSource.h"
#include "ui/navigation/NavigationFlyout.h"
#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationPushButton.h"
#include "ui/navigation/NavigationToolButton.h"
#include "ui/navigation/NavigationTreeItem.h"
#include "ui/navigation/NavigationTreeWidget.h"
#include "ui/navigation/NavigationWidget.h"

namespace ui::navigation {
    NavigationPanel::NavigationPanel(QWidget *parent)
        : QWidget(parent) {
        setupUi();
    }

    void NavigationPanel::setupUi() {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setAutoFillBackground(false);

        const auto s = themeSpacing();
        m_layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
        m_layout->setContentsMargins(0, 0, 0, s.small);
        m_layout->setSpacing(0);

        m_headerLayout = new QBoxLayout(QBoxLayout::TopToBottom);
        m_headerLayout->setContentsMargins(0, s.xSmall, 0, 0);
        m_headerLayout->setSpacing(0);

        m_backButton = new NavigationToolButton(QString(), this);
        m_backButton->setAccessibleItemName(QStringLiteral("Back"));
        m_backButton->setVisible(false);
        connect(m_backButton, &NavigationPushButton::clicked, this, &NavigationPanel::backRequested);
        m_headerLayout->addWidget(m_backButton, 0, Qt::AlignLeft);

        m_animatedBackIcon = new ui::animation::AnimatedIcon(m_backButton);
        m_animatedBackIcon->setSource(std::make_shared<ui::animation::AnimatedBackVisualSource>());

        m_paneToggleButton = new NavigationToolButton(QString(), this);
        m_paneToggleButton->setAccessibleItemName(QStringLiteral("Toggle navigation pane"));
        connect(m_paneToggleButton, &NavigationPushButton::clicked, this, &NavigationPanel::togglePane);
        m_headerLayout->addWidget(m_paneToggleButton, 0, Qt::AlignLeft);

        m_animatedPaneToggleIcon = new ui::animation::AnimatedIcon(m_paneToggleButton);
        m_animatedPaneToggleIcon->setSource(std::make_shared<ui::animation::AnimatedGlobalNavVisualSource>());
        m_layout->addLayout(m_headerLayout);

        m_tree = new NavigationTreeWidget(this);
        m_layout->addWidget(m_tree, 1);

        m_indicator = new NavigationIndicator(this);
        m_indicator->hide();

        connect(this, &NavigationPanel::indicatorOwnerChanged, m_tree, &NavigationTreeWidget::onIndicatorOwnerChanged);

        connect(m_tree, &NavigationTreeWidgetBase::expansionChanged, this,
                [this](const QString &routeKey, bool /*expanded*/) {
                    if (m_indicatorOwner &&m_tree->isAncestorOf(m_indicatorOwner->routeKey(), routeKey))
                    {
                        refreshIndicatorVisuals(true);
                    }
                });

        connect(m_tree, &NavigationTreeWidget::itemSelected, this,
                [this](const QString &routeKey) {
                    emit itemSelected(routeKey);
                });
        connect(m_tree, &NavigationTreeWidget::categoryActivated, this,
                [this](const QString &categoryKey, QWidget *anchorWidget) {
                    showFlyoutMenu(categoryKey, anchorWidget);
                });
        connect(m_tree, &NavigationTreeWidget::categoryDeactivated, this,
                [this](const QString & /*categoryKey*/) {
                    closeFlyoutMenu(true);
                });
        connect(m_tree, &NavigationTreeWidget::overflowMenuRequested, this,
                [this](QWidget *anchorWidget,
                       const QVector<NavigationOverflowEntry> &entries) {
                    showOverflowMenu(anchorWidget, entries);
                });

        m_paneFooterContainer = new QWidget(this);
        m_paneFooterLayout = new QBoxLayout(QBoxLayout::TopToBottom, m_paneFooterContainer);
        m_paneFooterLayout->setContentsMargins(0, 0, 0, 0);
        m_paneFooterLayout->setSpacing(0);
        m_paneFooterContainer->hide();
        m_layout->addWidget(m_paneFooterContainer);
    }

    void NavigationPanel::addItem(const QString &routeKey, const QString &iconGlyph,
                                  const QString &text, const QString &parentKey,
                                  NavigationItemPosition position, bool selectable, const QString &tooltip,
                                  std::shared_ptr<ui::animation::AnimatedVisualSource> visualSource) {
        if (!m_tree)
            return;
        m_tree->addItem(routeKey, iconGlyph, text, parentKey, position, selectable, tooltip, visualSource);
    }

    void NavigationPanel::addSectionHeader(const QString &text) {
        m_tree->addSectionHeader(text);
    }

    void NavigationPanel::addWidget(NavigationWidget *widget, NavigationItemPosition position) {
        m_tree->addWidget(widget, position);
    }


    void NavigationPanel::setCurrentItem(const QString &routeKey) {
        if (!m_tree || routeKey.isEmpty()) return;
        NavigationTreeItem *prevOwner = m_indicatorOwner;
        m_tree->setCurrentItem(routeKey, false);
        NavigationTreeWidget *node = m_tree->nodeFor(routeKey);
        NavigationTreeItem *curOwner = nullptr;
        if (node && node->itemWidget()) {
            curOwner = qobject_cast<NavigationTreeItem *>(node->itemWidget());
        }
        m_indicatorOwner = curOwner;
        dispatchIndicatorAnimation(prevOwner, curOwner);
    }

    void NavigationPanel::setCategoryExpanded(const QString &routeKey, bool expanded, bool animated) {
        if (m_tree)
            m_tree->setCategoryExpanded(routeKey, expanded, animated);
    }

    QString NavigationPanel::currentRouteKey() const {
        return m_tree ? m_tree->currentRouteKey() : QString();
    }

    void NavigationPanel::setPaneFooter(NavigationWidget *footerWidget) {
        if (m_paneFooter == footerWidget)
            return;

        if (m_paneFooter) {
            m_paneFooterLayout->removeWidget(m_paneFooter);
            m_paneFooter->setParent(nullptr);
        }

        m_paneFooter = footerWidget;

        if (m_paneFooter) {
            m_paneFooter->setItemPosition(NavigationItemPosition::Bottom);
            m_paneFooter->setOrientation(m_orientation);
            m_paneFooter->setCompacted(m_isCompacted);
            m_paneFooterLayout->addWidget(m_paneFooter);
            m_paneFooterContainer->show();
        } else {
            m_paneFooterContainer->hide();
        }
        updateGeometry();
    }

    bool NavigationPanel::isBackButtonVisible() const {
        return m_backButton && m_backButton->isVisible();
    }

    void NavigationPanel::setBackButtonVisible(bool visible) {
        if (m_backButton) {
            bool changed = (m_backButton->isVisibleTo(this) != visible);
            m_backButton->setVisible(visible);
            if (changed) {
                updateGeometry();
            }
        }
    }

    bool NavigationPanel::isBackEnabled() const {
        return m_backButton && m_backButton->isEnabled();
    }

    void NavigationPanel::setBackEnabled(bool enabled) {
        if (m_backButton &&m_backButton->isEnabled() != enabled)
        {
            m_backButton->setEnabled(enabled);
        }
    }

    bool NavigationPanel::isPaneToggleButtonVisible() const {
        return m_paneToggleButton && !m_paneToggleButton->isHidden();
    }

    void NavigationPanel::setPaneToggleButtonVisible(bool visible) {
        m_paneToggleExplicitlyHidden = !visible;
        if (m_paneToggleButton) {
            bool changed = (m_paneToggleButton->isVisibleTo(this) != visible);
            m_paneToggleButton->setVisible(visible);
            if (changed) {
                updateGeometry();
            }
        }
    }

    void NavigationPanel::setCompacted(bool compacted) {
        if (m_isCompacted == compacted)
            return;
        m_isCompacted = compacted;
        if (!compacted)
            closeFlyoutMenu(true);

        if (QWidget *focused = QApplication::focusWidget()) {
            if (isAncestorOf(focused)) {
                if (!NavigationWidget::isKeyboardMode()) {
                    focused->clearFocus();
                }
            }
        }

        if (m_tree)
            m_tree->setCompacted(compacted);
        if (m_paneToggleButton)
            m_paneToggleButton->setCompacted(compacted);
        if (m_backButton)
            m_backButton->setCompacted(compacted);
        if (m_paneFooter)
            m_paneFooter->setCompacted(compacted);

        refreshIndicatorVisuals();

        emit compactedChanged(m_isCompacted);
    }

    bool NavigationPanel::selectionFollowsFocus() const {
        return m_tree ? m_tree->selectionFollowsFocus() : false;
    }

    void NavigationPanel::setSelectionFollowsFocus(bool follows) {
        if (m_tree &&m_tree->selectionFollowsFocus() != follows)
        {
            m_tree->setSelectionFollowsFocus(follows);
        }
    }

    void NavigationPanel::togglePane() {
        setCompacted(!m_isCompacted);
    }

    void NavigationPanel::refreshIndicatorVisuals(bool animated, NavigationTreeItem *prevLogicalOwner) {
        if (!m_indicator || !m_tree || !m_indicatorOwner) return;

        // 获取逻辑 owner 对应的真实视觉代理
        NavigationTreeItem *prevVisual = prevLogicalOwner ? m_tree->getVisualProxyFor(prevLogicalOwner) : nullptr;
        NavigationTreeItem *curVisual = m_indicatorOwner ? m_tree->getVisualProxyFor(m_indicatorOwner) : nullptr;

        NavigationWidget *prevProxy = prevVisual ? prevVisual : (prevLogicalOwner ? prevLogicalOwner : nullptr);
        NavigationWidget *curProxy = curVisual ? curVisual : (m_indicatorOwner ? m_indicatorOwner.data() : nullptr);

        if (!prevProxy || !curProxy) {
            animated = false;
        }

        // 如果之前的 proxy 不可见，强制降级为无动画瞬发（防止坐标乱飞）
        if (prevProxy && !prevProxy->isVisibleTo(this)) {
            animated = false;
        }

        const QRectF targetRect = m_tree->indicatorRectInHost(curProxy, this);

        if (!curProxy || !curProxy->isVisibleTo(this)) {
            m_indicator->activateAt(targetRect, false);
            return;
        }

        QRectF startRect;
        if (animated && m_indicator->currentRect().isValid() && m_indicator->currentRect().width() > 0) {
            startRect = m_indicator->currentRect();
        } else {
            startRect = m_tree->indicatorRectInHost(prevProxy, this);
        }

        // 常显指示条转移
        if (m_visualIndicatorOwner) {
            m_visualIndicatorOwner->setShowIndicator(false);
        }
        if (curVisual) {
            m_visualIndicatorOwner = curVisual;
        } else {
            m_visualIndicatorOwner = m_indicatorOwner;
        }

        // 接管所有动画生命周期信号，防止信号交错
        m_indicator->disconnect(SIGNAL(flightFinished()));
        m_indicator->disconnect(SIGNAL(flightStarted()));

        connect(m_indicator, &NavigationIndicator::flightStarted, this, [this, prevLogicalOwner]() {
            if (prevLogicalOwner) emit indicatorOwnerChanged(prevLogicalOwner, false);
        }, Qt::SingleShotConnection);

        connect(m_indicator, &NavigationIndicator::flightFinished, this, [this]() {
            if (m_visualIndicatorOwner) m_visualIndicatorOwner->setShowIndicator(true);
            if (m_indicatorOwner) emit indicatorOwnerChanged(m_indicatorOwner, true);
        }, Qt::SingleShotConnection);

        if (animated) {
            if (startRect.isValid()) {
                m_indicator->setInitialPosition(startRect);
            }
            m_indicator->activateAt(targetRect, true);
        } else {
            m_indicator->activateAt(targetRect, false);
        }
    }

    void NavigationPanel::moveFocusBy(int delta) {
        if (m_tree)
            m_tree->moveFocusBy(delta);
    }

    QSize NavigationPanel::sizeHint() const {
        if (m_orientation == Qt::Horizontal) {
            return QSize(QWIDGETSIZE_MAX, kTopBarItemHeight);
        }
        const int w = m_isCompacted
                          ? Breakpoints::NavigationPaneCompactWidth
                          : Breakpoints::NavigationPaneExpandedWidth;
        return QSize(w, m_layout ? m_layout->sizeHint().height() : QWidget::sizeHint().height());
    }

    void NavigationPanel::paintEvent(QPaintEvent *event) {
        // 浮层抽屉模式透出下层毛玻璃，不填充背景
        if (!m_surfaceVisible) {
            const QColor fill = fluent::windowing::windowChromeBackdropFill(
                *this, window(), window() && window()->isActiveWindow());
            if (fill.isValid()) {
                QPainter painter(this);
                painter.fillRect(rect(), fill);
            }
        }
        QWidget::paintEvent(event);
    }

    void NavigationPanel::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);
        if (m_indicator)
            m_indicator->raise();

        // 首次展示防御回退：若当前完全没有任何选中的项，自动选中树中第一个可用的项（静默秒开无动画）
        if (!m_indicatorOwner && m_tree) {
            if (NavigationTreeItem *firstSelectable = m_tree->findFirstSelectableItem()) {
                setCurrentItem(firstSelectable->routeKey());
            }
        } else if (m_indicatorOwner) {
            refreshIndicatorVisuals(false);
        }
    }

    void NavigationPanel::setOrientation(Qt::Orientation orientation) {
        if (m_orientation == orientation)
            return;
        m_orientation = orientation;

        const auto direction = (orientation == Qt::Horizontal)
                                   ? QBoxLayout::LeftToRight
                                   : QBoxLayout::TopToBottom;

        m_layout->setDirection(direction);
        m_headerLayout->setDirection(direction);
        m_paneFooterLayout->setDirection(direction);

        // 重置尺寸约束，避免固定宽度污染外层 NavigationView 几何布局
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

        const auto s = themeSpacing();
        if (orientation == Qt::Horizontal) {
            m_paneToggleButton->hide();
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setFixedHeight(kTopBarItemHeight);
            // Horizontal 模式：外层布局边距清零，使导航项占满顶栏 48px 高度
            m_layout->setContentsMargins(0, 0, 0, 0);
            m_headerLayout->setContentsMargins(s.small, 0, 0, 0);
            // paneFooter 靠右：在横向布局末尾添加伸缩占位
            m_layout->setStretch(2, 0);
        } else {
            if (!m_paneToggleExplicitlyHidden) {
                m_paneToggleButton->show();
            }
            setMaximumHeight(QWIDGETSIZE_MAX);
            setMinimumHeight(0);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            m_layout->setContentsMargins(0, 0, 0, s.small);
            m_headerLayout->setContentsMargins(0, s.xSmall, 0, 0);
        }

        if (m_tree) {
            m_tree->setOrientation(orientation);
        }
        if (m_paneFooter) {
            m_paneFooter->setOrientation(orientation);
        }
        if (m_indicator) {
            m_indicator->setOrientation(orientation);
        }

        refreshIndicatorVisuals(false);
        updateGeometry();
    }

    bool NavigationPanel::event(QEvent *event) {
        if (event->type() == QEvent::WindowDeactivate) {
            if (m_activeFlyout && (qApp->activeWindow() == m_activeFlyout.data() || m_activeFlyout->isAncestorOf(
                                       qApp->focusWidget()))) {
                // 不在焦点进入 flyout 时误关闭
            } else {
                closeFlyoutMenu(true);
            }
        }
        if (event->type() == QEvent::DynamicPropertyChange) {
            if (auto *change = static_cast<QDynamicPropertyChangeEvent *>(event);
                change && change->propertyName() == "fluentNavPaneFloating") {
                setSurfaceVisible(property("fluentNavPaneFloating").toBool());
            }
        }
        return QWidget::event(event);
    }

    void NavigationPanel::setSurfaceVisible(bool visible) {
        if (m_surfaceVisible == visible)
            return;
        m_surfaceVisible = visible;
        setAttribute(Qt::WA_NoSystemBackground, visible);
        setAttribute(Qt::WA_TranslucentBackground, visible);
        update();
    }

    void NavigationPanel::triggerCrossWindowPortal(NavigationFlyout *flyout, QWidget *anchorWidget,
                                                   NavigationTreeItem *prevOwner, NavigationTreeItem *curClone) {
        if (m_orientation != Qt::Horizontal || !m_indicator || !flyout || !curClone)
            return;

        m_flyoutCloseIntent = FlyoutCloseIntent::PortalReturn;

        NavigationTreeItem *owner = m_indicatorOwner;
        NavigationTreeItem *visualOwner = m_tree->getVisualProxyFor(prevOwner ? prevOwner : owner);

        // 顶栏起飞矩形：取指示条当前的真实视觉位置而非选中项几何，保证从任意项起飞都从原位开始
        QRectF topHostStartRect;
        if (m_indicator &&m_indicator->currentRect().isValid() && m_indicator->currentRect().width() > 0)
        {
            topHostStartRect = m_indicator->currentRect();
        }
        else
        {
            NavigationWidget *topItem = visualOwner ? visualOwner : qobject_cast<NavigationWidget *>(anchorWidget);
            if (!topItem && owner) topItem = owner;
            topHostStartRect = m_tree->indicatorRectInHost(topItem, this);
        }

        // flyout 已定位，用 mapToGlobal 直接取真实全局几何（含翻转/clamp 后的最终位置）
        NavigationTreeItem *visualTarget = flyout->getVisualProxyFor(curClone);
        if (!visualTarget) visualTarget = curClone;
        const QRectF flyoutRect = visualTarget->indicatorRect();
        const QPointF globalTargetTopLeft = visualTarget->mapToGlobal(flyoutRect.topLeft().toPoint());
        const QRectF globalTargetRect(globalTargetTopLeft, flyoutRect.size());

        const QPointF globalStartTopLeft = this->mapToGlobal(topHostStartRect.topLeft().toPoint());
        const QRectF globalStartRect(globalStartTopLeft, topHostStartRect.size());

        // 顶栏指示条：从顶栏当前原位置向左收缩（Horizontal 宿主收缩分支）
        const QRectF topHostTargetRect(this->mapFromGlobal(globalTargetRect.topLeft().toPoint()),
                                       globalTargetRect.size());

        // flyout 指示条：从目标位置顶部向下生长（Vertical 宿主生长分支）
        const QRectF flyoutHostStartRect(flyout->mapFromGlobal(globalStartRect.topLeft().toPoint()),
                                         globalStartRect.size());
        const QRectF itemInFlyoutHost = flyout->indicatorRectInHost(curClone);

        // 顶栏熄灭逻辑：顶栏指示条起飞时，熄灭原节点的常驻指示条
        connect(m_indicator, &NavigationIndicator::flightStarted, this, [this, prevOwner, owner]() {
            NavigationTreeItem *itemToExtinguish = prevOwner ? prevOwner : owner;
            if (itemToExtinguish) emit indicatorOwnerChanged(itemToExtinguish, false);
            if (m_visualIndicatorOwner) {
                m_visualIndicatorOwner->setShowIndicator(false);
                m_visualIndicatorOwner = nullptr;
            }
        }, Qt::SingleShotConnection);

        m_indicator->playCrossWindowPortal(topHostStartRect, topHostTargetRect);

        // 浮层点亮逻辑：浮层指示条降落后，点亮最终节点的常驻指示条
        if (NavigationIndicator *flyoutIndicator = flyout->indicator()) {
            connect(flyoutIndicator, &NavigationIndicator::flightFinished, this, [this, flyout, curClone]() {
                if (m_indicatorOwner) emit indicatorOwnerChanged(m_indicatorOwner, true);
            }, Qt::SingleShotConnection);
        }

        // 使用 indicatorRectInHost 作为真正的降落点，以支持折叠父节点的代理回退
        flyout->playSelectedItemCrossPortal(curClone, flyoutHostStartRect, itemInFlyoutHost);
    }

    void NavigationPanel::showFlyoutMenu(const QString &categoryKey, QWidget *anchorWidget) {
        if (!m_tree || !anchorWidget)
            return;

        const QRect anchorRect(anchorWidget->mapTo(this, QPoint(0, 0)), anchorWidget->size());

        closeFlyoutMenu(false);

        auto *flyout = new NavigationFlyout(m_tree, this);
        flyout->rebuildSubtree(categoryKey);

        connect(flyout, &NavigationFlyout::aboutToShow, this, [this, flyout]() {
            m_activeFlyout = flyout;
        });
        connect(flyout, &NavigationFlyout::aboutToHide, this, [this, flyout]() {
            if (m_activeFlyout == flyout) m_activeFlyout = nullptr;
        });

        connect(this, &NavigationPanel::indicatorOwnerChanged, flyout, &NavigationFlyout::onIndicatorOwnerChanged);

        // flyout 内展开状态改变：如果改变的是当前选中项的祖先，重新派发内部滑动动画
        connect(flyout, &NavigationFlyout::expansionChanged, this,
                [this, flyoutPtr = QPointer<NavigationFlyout>(flyout)](const QString &routeKey, bool /*expanded*/) {
                    if (flyoutPtr && m_indicatorOwner && m_tree->isAncestorOf(m_indicatorOwner->routeKey(), routeKey)) {
                        QTimer::singleShot(0, this, [this, flyoutPtr]() {
                            if (flyoutPtr) {
                                dispatchIndicatorAnimation(m_indicatorOwner, m_indicatorOwner);
                            }
                        });
                    }
                });

        // flyout 关闭（无论何种方式）时统一收尾：重排 + 按意图播放动画
        // 使用 aboutToHide 消除 150ms 滞后感，使主面板指示条与浮层淡出同时归位
        connect(flyout, &NavigationFlyout::aboutToHide, this,
                [this, categoryKey, anchorWidget]() {
                    animateFlyoutClosed(categoryKey, anchorWidget);
                });

        // Horizontal 模式下：若选中项属于当前分类子树，待 flyout 完全定位后触发跨窗口联合传送门
        // 须在 opened 后触发：flyout 定位/布局完成前 selectedItem 几何不准，且可能翻转（m_flippedUp）
        if (m_orientation == Qt::Horizontal && m_indicator && m_tree->isAncestorOf(
                m_tree->currentRouteKey(), categoryKey)) {
            connect(flyout, &NavigationFlyout::opened, this,
                    [this, flyout, anchorWidget]() {
                        if (NavigationTreeItem *curClone = flyout->cloneItemFor(m_tree->currentRouteKey())) {
                            triggerCrossWindowPortal(flyout, anchorWidget, m_indicatorOwner, curClone);
                        }
                    }, Qt::SingleShotConnection);
        }

        flyout->addLightDismissPassthrough(this); // 允许侧边栏内的点击直接穿透（实现无缝切换）

        if (m_orientation == Qt::Horizontal) {
            // Horizontal 模式：flyout 顶部紧贴 panel 下外边缘，X 对齐锚点水平中心，向下滑入
            flyout->setPlacement(NavigationFlyout::Placement::Bottom);
            flyout->showAt(anchorWidget);
        } else {
            // Vertical 紧凑模式：flyout 左边紧贴 panel 右外边缘，Y 垂直居中于锚点
            flyout->setPlacement(NavigationFlyout::Placement::Right);
            flyout->showAt(anchorWidget);
        }
    }

    void NavigationPanel::closeFlyoutMenu(bool animated) {
        if (!m_activeFlyout)
            return;
        auto *flyout = m_activeFlyout.data();

        if (animated && flyout->isVisible()) {
            connect(flyout, &NavigationFlyout::closed,
                    flyout, &QObject::deleteLater);
            flyout->close();
        } else {
            // 无动画时也必须走 close() 流程以保证 aboutToHide -> closed 生命周期闭环
            flyout->setExitAnimationEnabled(false);
            connect(flyout, &NavigationFlyout::closed,
                    flyout, &QObject::deleteLater);
            flyout->close();
        }
    }

    void NavigationPanel::showOverflowMenu(QWidget *anchorWidget,
                                           const QVector<NavigationOverflowEntry> &entries) {
        if (!anchorWidget || entries.isEmpty())
            return;

        const QRect anchorRect(anchorWidget->mapTo(this, QPoint(0, 0)), anchorWidget->size());

        closeOverflowMenu(false);

        auto *flyout = new NavigationFlyout(m_tree, this);
        flyout->rebuildSubtreeFromEntries(entries);

        connect(flyout, &NavigationFlyout::aboutToShow, this, [this, flyout]() {
            m_activeFlyout = flyout;
        });
        connect(flyout, &NavigationFlyout::aboutToHide, this, [this, flyout]() {
            if (m_activeFlyout == flyout) m_activeFlyout = nullptr;
        });

        connect(this, &NavigationPanel::indicatorOwnerChanged, flyout, &NavigationFlyout::onIndicatorOwnerChanged);

        flyout->addLightDismissPassthrough(this);

        // flyout 内展开状态改变：如果改变的是当前选中项的祖先，重新派发内部滑动动画
        connect(flyout, &NavigationFlyout::expansionChanged, this,
                [this, flyoutPtr = QPointer<NavigationFlyout>(flyout)](const QString &routeKey, bool /*expanded*/) {
                    if (flyoutPtr && m_indicatorOwner && m_tree->isAncestorOf(m_indicatorOwner->routeKey(), routeKey)) {
                        QTimer::singleShot(0, this, [this, flyoutPtr]() {
                            if (flyoutPtr) {
                                dispatchIndicatorAnimation(m_indicatorOwner, m_indicatorOwner);
                            }
                        });
                    }
                });

        // overflow flyout 关闭后统一收尾：重排 + 按意图播放动画
        connect(flyout, &NavigationFlyout::aboutToHide, this,
                [this, anchorWidget]() {
                    animateFlyoutClosed(QString(), anchorWidget);
                });

        // Horizontal 模式下：若选中项在溢出列表中，待 flyout 完全定位后触发跨窗口联合传送门
        if (m_orientation == Qt::Horizontal && m_indicator) {
            connect(flyout, &NavigationFlyout::opened, this,
                    [this, flyout, anchorWidget]() {
                        if (NavigationTreeItem *curClone = flyout->cloneItemFor(m_tree->currentRouteKey())) {
                            triggerCrossWindowPortal(flyout, anchorWidget, m_indicatorOwner, curClone);
                        }
                    }, Qt::SingleShotConnection);
        }

        flyout->setPlacement(NavigationFlyout::Placement::BottomRight);
        flyout->showAt(anchorWidget);
    }

    void NavigationPanel::closeOverflowMenu(bool animated) {
        if (!m_activeFlyout)
            return;
        auto *flyout = m_activeFlyout.data();

        if (animated && flyout->isVisible()) {
            connect(flyout, &NavigationFlyout::closed,
                    flyout, &QObject::deleteLater);
            flyout->close();
        } else {
            flyout->setExitAnimationEnabled(false);
            connect(flyout, &NavigationFlyout::closed,
                    flyout, &QObject::deleteLater);
            flyout->close();
        }
    }

    void NavigationPanel::dispatchIndicatorAnimation(NavigationTreeItem *prevOwner, NavigationTreeItem *curOwner) {
        if (!curOwner)
            return;

        // 获取当前处于激活状态的 Flyout
        NavigationFlyout *activeFlyout = m_activeFlyout.data();

        // O(1) 查找 Flyout 内部是否有克隆替身
        NavigationTreeItem *prevClone = activeFlyout && prevOwner
                                            ? activeFlyout->cloneItemFor(prevOwner->routeKey())
                                            : nullptr;
        NavigationTreeItem *curClone = activeFlyout ? activeFlyout->cloneItemFor(curOwner->routeKey()) : nullptr;

        if (activeFlyout && curClone && !curOwner->isCategory() && prevOwner != curOwner) {
            // 判决0（最高优先级）：真正的叶子项点击切换（非同节点状态刷新），Flyout 即将关闭，
            // 检查前驱项是否也在当前浮层内：
            // - 若 prevClone 也存在，说明前驱项与当前项均属于该浮层（共享同一主树分类代理或 overflow 锚点），
            //   主面板无需跨项滑动，归位走 PortalReturn；
            // - 若 prevClone 为空，说明前驱项在主树外部，主面板需执行 LeafSlide 跨项滑动。
            if (prevClone) {
                m_flyoutCloseIntent = FlyoutCloseIntent::PortalReturn;
            } else {
                m_flyoutCloseIntent = FlyoutCloseIntent::LeafSlide;
            }
            m_flyoutPrevOwner = prevOwner;
            if (prevOwner) emit indicatorOwnerChanged(prevOwner, false);
            return;
        }

        if (activeFlyout && prevClone && curClone) {
            // 判决1：同 flyout 内飞跃（仅针对可选分类项的 Inline Expansion）
            NavigationIndicator *flyoutInd = activeFlyout->indicator();
            if (!flyoutInd) return;

            // 同步起点：如果是同节点展开/折叠代理变更，不强制同步，允许从原视觉位置平滑滑过去
            if (prevOwner && prevOwner != curOwner) {
                const QRectF startRect = activeFlyout->indicatorRectInHost(prevClone);
                flyoutInd->setInitialPosition(startRect);
            }

            connect(flyoutInd, &NavigationIndicator::flightStarted, this, [this, prevOwner]() {
                if (prevOwner) emit indicatorOwnerChanged(prevOwner, false);
            }, Qt::SingleShotConnection);
            connect(flyoutInd, &NavigationIndicator::flightFinished, this, [this, curOwner]() {
                if (m_indicatorOwner == curOwner) emit indicatorOwnerChanged(curOwner, true);
            }, Qt::SingleShotConnection);

            const QRectF targetRect = activeFlyout->indicatorRectInHost(curClone);
            activeFlyout->playInternalFlight(targetRect);
        } else if (activeFlyout && !prevClone && curClone) {
            // 判决2：跨窗口传送门（仅针对可选分类项）
            QWidget *anchor = activeFlyout->anchor();
            triggerCrossWindowPortal(activeFlyout, anchor, prevOwner, curClone);
        } else {
            // 判决3：主界面常规飞跃或视觉回退
            if (!m_indicator) return;

            if (!prevOwner) {
                refreshIndicatorVisuals(false);
                return;
            }

            // 完全贯通：起点同步与所有动画生命周期的信号分发均收敛到 refreshIndicatorVisuals 中
            refreshIndicatorVisuals(true, prevOwner);
        }
    }

    void NavigationPanel::animateFlyoutClosed(const QString &categoryKey, QWidget *anchorWidget) {
        if (!m_tree) {
            m_flyoutCloseIntent = FlyoutCloseIntent::None;
            m_flyoutPrevOwner = nullptr;
            return;
        }

        // ① 先重排（保证后续所有坐标计算基于最新布局）
        if (!categoryKey.isEmpty()) m_tree->dismissCategory();
        m_tree->updateOverflow();

        // ② 实时重读 owner（防止用户在 flyout 动画期间又点了别的）
        NavigationTreeItem *logicOwner = m_indicatorOwner;
        if (!logicOwner) {
            m_flyoutCloseIntent = FlyoutCloseIntent::None;
            m_flyoutPrevOwner = nullptr;
            return;
        }

        // ③ 取出意图与 prevOwner 后复位，防止多开 flyout 状态残留
        const FlyoutCloseIntent intent = m_flyoutCloseIntent;
        NavigationTreeItem *prevOwner = m_flyoutPrevOwner;
        m_flyoutCloseIntent = FlyoutCloseIntent::None;
        m_flyoutPrevOwner = nullptr;

        // ④ 延迟到下一帧计算（等待 updateOverflow 与 dismissCategory 完成布局刷新）
        QTimer::singleShot(0, this, [this, categoryKey, anchorWidget, logicOwner, intent, prevOwner]() {
            // 在延时回调中再次确认 owner 是否改变
            if (m_indicatorOwner != logicOwner) {
                return;
            }

            const bool belongsToFlyout = categoryKey.isEmpty()
                                             ? m_tree->hasOverflowItems()
                                             : (logicOwner->routeKey() == categoryKey
                                                || m_tree->isAncestorOf(logicOwner->routeKey(), categoryKey));

            if (!belongsToFlyout) {
                emit indicatorOwnerChanged(logicOwner, true);
                refreshIndicatorVisuals(false);
                return;
            }

            // 计算终点：owner 在顶栏上的视觉代理
            NavigationTreeItem *visualOwner = m_tree->getVisualProxyFor(logicOwner);
            NavigationWidget *returnItem = visualOwner
                                               ? visualOwner
                                               : qobject_cast<NavigationWidget *>(anchorWidget);
            if (!returnItem) returnItem = logicOwner;
            const QRectF targetRect = m_tree->indicatorRectInHost(returnItem, this);

            // 按意图分流动画
            switch (intent) {
                case FlyoutCloseIntent::LeafSlide: {
                    // 完全贯通：交由 refreshIndicatorVisuals 统一处理同步、动画与信号分发
                    if (m_indicator) {
                        if (prevOwner) {
                            NavigationTreeItem *prevVisual = m_tree->getVisualProxyFor(prevOwner);
                            if (prevVisual) {
                                m_indicator->setInitialPosition(m_tree->indicatorRectInHost(prevVisual, this));
                            }
                        }
                        refreshIndicatorVisuals(true, prevOwner);
                    }
                    break;
                }
                case FlyoutCloseIntent::PortalReturn: {
                    // 仅对需要特殊动画的 PortalReturn 注册单独的降落亮灯
                    connect(m_indicator, &NavigationIndicator::flightFinished, this,
                            [this, logicOwner]() {
                                if (m_indicatorOwner == logicOwner) {
                                    emit indicatorOwnerChanged(logicOwner, true);
                                    refreshIndicatorVisuals(false);
                                }
                            }, Qt::SingleShotConnection);
                    m_indicator->playPortalReturn(targetRect);
                    break;
                }
                case FlyoutCloseIntent::None: {
                    refreshIndicatorVisuals(false, prevOwner);
                    break;
                }
            }
        });
    }
} // namespace ui::navigation
