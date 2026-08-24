#include "ui/window/NavigationWindow.h"
#include <FluentQt/Navigation.h>
#include <QDebug>

#include "ui/navigation/NavigationHistory.h"
#include "ui/navigation/NavigationIndicator.h"
#include "ui/navigation/NavigationPanel.h"
#include "ui/navigation/NavigationToolButton.h"
#include "ui/navigation/NavigationWidget.h"
#include "ui/window/TitleBar.h"

NavigationWindow::NavigationWindow(QWidget *parent)
    : WindowBase(parent) {
    initNavigation();
}

NavigationWindow::~NavigationWindow() = default;

void NavigationWindow::initNavigation() {
    m_navigationView = new fluent::navigation::NavigationView(this);
    m_navigationView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Auto);

    m_history = new ui::navigation::NavigationHistory(this);

    m_panel = new ui::navigation::NavigationPanel(m_navigationView);
    m_navigationView->setMainChromeWidget(m_panel);

    // 隐藏 NavigationPanel 自带的按钮，由 TitleBar 接管
    m_panel->setPaneToggleButtonVisible(false);
    m_panel->setBackButtonVisible(false);

    auto syncDisplayMode = [this](fluent::navigation::NavigationView::DisplayMode mode) {
        using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
        using StackContentHost = fluent::navigation::StackContentHost;
        const bool top = (mode == DisplayMode::Top);
        if (m_panel) {
            m_panel->setOrientation(top ? Qt::Horizontal : Qt::Vertical);
        }
        if (m_navigationView) {
            m_navigationView->setPaneOpen(mode == DisplayMode::Left || top);
            if (auto *host = m_navigationView->contentHost()) {
                host->setTransitionEffect(
                    top
                        ? StackContentHost::TransitionEffect::SlideFromLeft
                        : StackContentHost::TransitionEffect::SlideFromBottom);
            }
        }
        if (m_panel && m_navigationView) {
            m_panel->setCompacted(!m_navigationView->isPaneOpen());
        }
        if (titleBar()) {
            titleBar()->setPaneToggleButtonVisible(!top);
        }
    };

    connect(m_navigationView, &fluent::navigation::NavigationView::effectiveDisplayModeChanged,
            this, syncDisplayMode);
    syncDisplayMode(m_navigationView->effectiveDisplayMode());

    connect(m_navigationView, &fluent::navigation::NavigationView::paneOpenChanged,
            this, [this](bool open) {
                if (m_panel) {
                    m_panel->setCompacted(!open);
                }
            });

    connect(m_panel, &ui::navigation::NavigationPanel::backRequested, this, [this]() {
        if (m_navigationView) {
            using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
            const auto mode = m_navigationView->effectiveDisplayMode();
            if (m_navigationView->isPaneOpen()
                && (mode == DisplayMode::LeftCompact || mode == DisplayMode::LeftMinimal)) {
                m_navigationView->setPaneOpen(false);
                return;
            }
        }

        if (m_history &&m_history->canGoBack())
        {
            m_isNavigatingHistory = true;
            QString prevRoute = m_history->goBack();
            switchTo(prevRoute);
            m_isNavigatingHistory = false;
        }
    });

    if (m_panel->paneToggleButton()) {
        connect(m_panel->paneToggleButton(), &ui::navigation::NavigationToolButton::clicked, this, [this]() {
            if (m_navigationView) {
                m_navigationView->setPaneOpen(!m_navigationView->isPaneOpen());
            }
        });
    }

    if (titleBar()) {
        bool canGoBack = m_history && m_history->canGoBack();
        titleBar()->setBackButtonVisible(canGoBack);
        titleBar()->setBackButtonEnabled(true);

        connect(m_history, &ui::navigation::NavigationHistory::canGoBackChanged,
                titleBar(), &ui::window::TitleBar::setBackButtonVisible);

        connect(titleBar(), &ui::window::TitleBar::backButtonClicked, this, [this]() {
            if (m_history &&m_history->canGoBack())
            {
                m_isNavigatingHistory = true;
                QString prevRoute = m_history->goBack();
                switchTo(prevRoute);
                m_isNavigatingHistory = false;
            }
        });

        connect(titleBar(), &ui::window::TitleBar::paneToggleButtonClicked, this, [this]() {
            if (m_navigationView) {
                m_navigationView->setPaneOpen(!m_navigationView->isPaneOpen());
            }
        });
    }

    syncDisplayMode(m_navigationView->effectiveDisplayMode());
    if (m_panel) {
        m_panel->setCompacted(!m_navigationView->isPaneOpen());
    }

    connect(m_panel, &ui::navigation::NavigationPanel::itemSelected, this, [this](const QString &routeKey) {
        switchTo(routeKey);
    });

    connect(m_navigationView->contentHost(), &fluent::navigation::StackContentHost::currentIndexChanged,
            this, [this](int index) {
                const QString routeKey = m_indexToRouteMap.value(index);
                if (!routeKey.isEmpty() && m_panel)
                    m_panel->setCurrentItem(routeKey);
                if (!routeKey.isEmpty()) {
                    Q_EMIT routeChanged(routeKey);
                }
            });

    setContentWidget(m_navigationView);
}

void NavigationWindow::addSectionHeader(const QString &text) {
    if (m_panel) {
        m_panel->addSectionHeader(text);
    }
}

void NavigationWindow::addWidget(ui::navigation::NavigationWidget *widget,
                                 ui::navigation::NavigationItemPosition position) {
    if (m_panel) {
        m_panel->addWidget(widget, position);
    }
}

void NavigationWindow::addSubInterface(
    const QString &routeKey,
    QWidget *interfaceWidget,
    const QString &iconGlyph,
    const QString &text,
    const QString &parentRouteKey,
    ui::navigation::NavigationItemPosition pos,
    bool selectable,
    std::shared_ptr<ui::animation::AnimatedVisualSource> visualSource) {
    if (!m_navigationView || !m_panel) return;

    if (interfaceWidget) {
        auto *contentHost = m_navigationView->contentHost();
        const int index = contentHost->count();
        contentHost->insertPage(index, interfaceWidget);
        m_routeToIndexMap.insert(routeKey, index);
        m_indexToRouteMap.insert(index, routeKey);
    }

    m_panel->addItem(routeKey, iconGlyph, text, parentRouteKey, pos,
                     selectable && (interfaceWidget != nullptr),
                     /*tooltip=*/QString(),
                     visualSource);
}

void NavigationWindow::addContentPage(const QString &routeKey, QWidget *interfaceWidget) {
    if (!m_navigationView || !interfaceWidget || routeKey.isEmpty() || m_routeToIndexMap.contains(routeKey)) return;

    auto *contentHost = m_navigationView->contentHost();
    const int index = contentHost->count();
    contentHost->insertPage(index, interfaceWidget);
    m_routeToIndexMap.insert(routeKey, index);
    m_indexToRouteMap.insert(index, routeKey);
    qDebug().noquote() << "[NavigationWindow] add content page route=" << routeKey << "index=" << index;
}

void NavigationWindow::switchTo(const QString &routeKey) {
    if (!m_routeToIndexMap.contains(routeKey) || !m_navigationView) return;

    if (!m_isNavigatingHistory && m_history) {
        m_history->push(routeKey);
    }

    auto *contentHost = m_navigationView->contentHost();
    const int targetIndex = m_routeToIndexMap.value(routeKey);
    const int currentIndex = contentHost->currentIndex();
    const int direction = targetIndex >= currentIndex ? 1 : -1;

    qDebug().noquote() << "[NavigationWindow] switch route=" << routeKey
                       << "from=" << currentIndex << "to=" << targetIndex;
    if (targetIndex == currentIndex) {
        Q_EMIT routeChanged(routeKey);
        return;
    }
    contentHost->setCurrentIndex(targetIndex, direction, true);
}

void NavigationWindow::setPaneFooter(ui::navigation::NavigationWidget *footerWidget) {
    if (!m_panel) return;
    m_panel->setPaneFooter(footerWidget);
}

ui::navigation::NavigationWidget *NavigationWindow::paneFooter() const {
    return m_panel ? m_panel->paneFooter() : nullptr;
}


