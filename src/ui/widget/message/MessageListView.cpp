#include "MessageCardWidget.h"
#include "MessageListView.h"
#include <QResizeEvent>
#include <QSet>

namespace ui::widget::message {

    MessageListView::MessageListView(QWidget* parent) : fluent::scrolling::ScrollView(parent) {
        setupUi();
    }

    MessageListView::~MessageListView() = default;

    void MessageListView::setupUi() {
        setFrameShape(QFrame::NoFrame);
        setWidgetResizable(true);

        setAutoFillBackground(false);
        if (QWidget* vp = viewport()) {
            vp->setAutoFillBackground(false);
            vp->setAttribute(Qt::WA_TranslucentBackground, false);
            vp->setAttribute(Qt::WA_OpaquePaintEvent, false);
            QPalette pal = vp->palette();
            pal.setColor(QPalette::Window, Qt::transparent);
            pal.setColor(QPalette::Base, Qt::transparent);
            vp->setPalette(pal);
        }

        setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Visible);

        m_container = new QWidget(this);
        m_container->setAutoFillBackground(false);
        m_container->setAttribute(Qt::WA_TranslucentBackground, false);
        m_container->setAttribute(Qt::WA_OpaquePaintEvent, false);
        QPalette cPal = m_container->palette();
        cPal.setColor(QPalette::Window, Qt::transparent);
        cPal.setColor(QPalette::Base, Qt::transparent);
        m_container->setPalette(cPal);

        m_layout = new QVBoxLayout(m_container);
        m_layout->setContentsMargins(16, 16, 16, 256);
        m_layout->setSpacing(16);
        m_layout->addStretch(1);

        setWidget(m_container);

        m_scrollAnimation = new QVariantAnimation(this);
        m_scrollAnimation->setDuration(150);
        m_scrollAnimation->setEasingCurve(QEasingCurve::OutQuad);
        connect(m_scrollAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            verticalScrollBar()->setValue(v.toInt());
            });

        bindScrollBarSignals(verticalScrollBar());

        m_followTimer = new QTimer(this);
        m_followTimer->setSingleShot(true);
        m_followTimer->setInterval(16);
        connect(m_followTimer, &QTimer::timeout, this, &MessageListView::executeFollowBottom);
    }

    void MessageListView::setCustomScrollBar(QScrollBar* scrollBar) {
        if (m_customScrollBar == scrollBar) return;

        if (m_customScrollBar) {
            m_customScrollBar->disconnect(this);
            verticalScrollBar()->disconnect(m_customScrollBar);
        }

        m_customScrollBar = scrollBar;

        if (!m_customScrollBar) {
            setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Visible);
            return;
        }

        setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

        QScrollBar* realBar = verticalScrollBar();

        m_customScrollBar->setRange(realBar->minimum(), realBar->maximum());
        m_customScrollBar->setPageStep(realBar->pageStep());
        m_customScrollBar->setSingleStep(realBar->singleStep());
        m_customScrollBar->setValue(realBar->value());

        connect(realBar, &QScrollBar::rangeChanged, m_customScrollBar, [this](int min, int max) {
            if (m_customScrollBar) {
                m_customScrollBar->setRange(min, max);
                m_customScrollBar->setVisible(max > min);
            }
            });

        connect(realBar, &QScrollBar::valueChanged, m_customScrollBar, [this](int val) {
            if (m_customScrollBar && m_customScrollBar->value() != val) {
                m_customScrollBar->setValue(val);
            }
            });

        connect(m_customScrollBar, &QScrollBar::valueChanged, realBar, [realBar](int val) {
            if (realBar->value() != val) {
                realBar->setValue(val);
            }
            });

        bindScrollBarSignals(m_customScrollBar);
    }

    void MessageListView::bindScrollBarSignals(QScrollBar* bar) {
        if (!bar) return;

        connect(bar, &QScrollBar::sliderPressed, this, [this]() {
            m_autoScrollToBottom = false;
            m_scrollAnimation->stop();
            });

        connect(bar, &QScrollBar::sliderReleased, this, [this, bar]() {
            if (bar->value() >= bar->maximum() - 10) {
                m_autoScrollToBottom = true;
            }
            });
    }

    void MessageListView::syncMessages(const QList<domain::conversation::Message>& messages) {
        QSet<QUuid> incomingIds;
        for (const auto& msg : messages) {
            incomingIds.insert(msg.id);
        }

        auto it = m_cardMap.begin();
        while (it != m_cardMap.end()) {
            if (!incomingIds.contains(it.key())) {
                MessageCardWidget* card = it.value();
                m_cardLastHeights.remove(card);
                m_layout->removeWidget(card);
                card->deleteLater();
                it = m_cardMap.erase(it);
            }
            else {
                ++it;
            }
        }

        for (qsizetype i = 0; i < messages.size(); ++i) {
            const auto& msg = messages.at(i);
            MessageCardWidget* card = nullptr;

            if (m_cardMap.contains(msg.id)) {
                card = m_cardMap[msg.id];
                card->syncMessage(msg);
            }
            else {
                card = new MessageCardWidget(msg, m_container);
                m_cardMap.insert(msg.id, card);

                connect(card, &MessageCardWidget::contentHeightChanged, this, &MessageListView::onCardHeightChanged);
            }

            m_cardLastHeights[card] = card->height();

            card->setAvatarVisible(m_avatarVisible);
            card->setHeaderVisible(m_headerVisible);

            if (m_layout->indexOf(card) != i) {
                m_layout->removeWidget(card);
                m_layout->insertWidget(i, card);
            }
        }

        updateScrollGeometry();
        scheduleFollowBottom();
    }

    void MessageListView::onCardHeightChanged() {
        auto* card = qobject_cast<MessageCardWidget*>(sender());
        if (card && viewport()) {
            const int oldH = m_cardLastHeights.value(card, card->height());
            const int newH = card->sizeHint().height();
            m_cardLastHeights[card] = newH;

            if (!m_autoScrollToBottom && oldH > 0 && newH != oldH) {
                const int cardY = card->mapTo(viewport(), QPoint(0, 0)).y();
                if (cardY + oldH <= 0) {
                    const int delta = newH - oldH;
                    if (delta != 0) {
                        QScrollBar* bar = verticalScrollBar();
                        bar->setValue(bar->value() + delta);
                    }
                }
            }
        }

        updateScrollGeometry();
        scheduleFollowBottom();
    }

    void MessageListView::updateScrollGeometry() {
        if (!m_container || !viewport()) return;
        const int targetWidth = viewport()->width();
        const int contentHeight = m_layout ? m_layout->sizeHint().height() : m_container->sizeHint().height();
        const int targetHeight = qMax(contentHeight, viewport()->height());
        if (m_container->width() != targetWidth || m_container->height() != targetHeight) {
            m_container->resize(targetWidth, targetHeight);
        }
    }

    void MessageListView::setAvatarVisible(bool visible) {
        if (m_avatarVisible == visible) return;
        m_avatarVisible = visible;
        for (auto it = m_cardMap.begin(); it != m_cardMap.end(); ++it) {
            it.value()->setAvatarVisible(visible);
        }
        updateScrollGeometry();
    }

    void MessageListView::setHeaderVisible(bool visible) {
        if (m_headerVisible == visible) return;
        m_headerVisible = visible;
        for (auto it = m_cardMap.begin(); it != m_cardMap.end(); ++it) {
            it.value()->setHeaderVisible(visible);
        }
        updateScrollGeometry();
    }

    void MessageListView::clear() {
        for (auto it = m_cardMap.begin(); it != m_cardMap.end(); ++it) {
            m_layout->removeWidget(it.value());
            it.value()->deleteLater();
        }
        m_cardMap.clear();
        m_cardLastHeights.clear();
        updateScrollGeometry();
    }

    void MessageListView::scrollToBottom() {
        m_autoScrollToBottom = true;
        scheduleFollowBottom();
    }

    void MessageListView::scheduleFollowBottom() {
        if (m_autoScrollToBottom && !m_followTimer->isActive()) {
            m_followTimer->start();
        }
    }

    void MessageListView::executeFollowBottom() {
        QScrollBar* bar = verticalScrollBar();
        const int target = bar->maximum();

        if (bar->value() >= target) {
            m_scrollAnimation->stop();
            return;
        }

        if (m_scrollAnimation->state() == QAbstractAnimation::Running) {
            m_scrollAnimation->stop();
            m_scrollAnimation->setDuration(100);
        }
        else {
            m_scrollAnimation->setDuration(150);
        }

        m_scrollAnimation->setStartValue(bar->value());
        m_scrollAnimation->setEndValue(target);
        m_scrollAnimation->start();
    }

    void MessageListView::wheelEvent(QWheelEvent* event) {
        if (event->angleDelta().y() > 0) {
            m_autoScrollToBottom = false;
            m_scrollAnimation->stop();
        }

        fluent::scrolling::ScrollView::wheelEvent(event);

        if (verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 10) {
            m_autoScrollToBottom = true;
        }
    }

    void MessageListView::resizeEvent(QResizeEvent* event) {
        fluent::scrolling::ScrollView::resizeEvent(event);
        updateScrollGeometry();
    }

    void MessageListView::showEvent(QShowEvent* event) {
        fluent::scrolling::ScrollView::showEvent(event);
        updateScrollGeometry();
        if (m_autoScrollToBottom) {
            QTimer::singleShot(0, this, &MessageListView::executeFollowBottom);
        }
    }

    void MessageListView::onThemeUpdated() {
        fluent::scrolling::ScrollView::onThemeUpdated();

        // 重新应用透明背景 (因为主题更新可能会重置 palette)
        if (QWidget* vp = viewport()) {
            QPalette pal = vp->palette();
            pal.setColor(QPalette::Window, Qt::transparent);
            pal.setColor(QPalette::Base, Qt::transparent);
            vp->setPalette(pal);
        }

        QPalette cPal = m_container->palette();
        cPal.setColor(QPalette::Window, Qt::transparent);
        cPal.setColor(QPalette::Base, Qt::transparent);
        m_container->setPalette(cPal);
    }

} // namespace ui::widget::message
