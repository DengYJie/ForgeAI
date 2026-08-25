#include "MessageListView.h"

#include "MessageCardWidget.h"

#include <QResizeEvent>
#include <QSet>

namespace ui::widget::message {
namespace {
constexpr int kHorizontalMargin = 16;
constexpr int kMessageContentMaxWidth = 1000;
constexpr int kTopMargin = 16;
constexpr int kBottomMargin = 256;
constexpr int kItemSpacing = 16;
constexpr int kPreloadViewports = 2;
}

MessageListView::MessageListView(QWidget* parent) : fluent::scrolling::ScrollView(parent) { setupUi(); }
MessageListView::~MessageListView() = default;

void MessageListView::setupUi()
{
    setFrameShape(QFrame::NoFrame);
    // The container height is controlled by virtual item geometry, not a
    // QScrollArea layout pass.
    setWidgetResizable(false);
    setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Visible);
    if (QWidget* view = viewport()) {
        view->setAutoFillBackground(false);
        view->setAttribute(Qt::WA_TranslucentBackground, true);
        view->setAttribute(Qt::WA_OpaquePaintEvent, false);
        QPalette palette = view->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        view->setPalette(palette);
        view->setStyleSheet(QStringLiteral("background: transparent;"));
    }
    m_container = new QWidget(this);
    m_container->setAutoFillBackground(false);
    m_container->setAttribute(Qt::WA_TranslucentBackground, true);
    m_container->setAttribute(Qt::WA_OpaquePaintEvent, false);
    QPalette palette = m_container->palette();
    palette.setColor(QPalette::Window, Qt::transparent);
    palette.setColor(QPalette::Base, Qt::transparent);
    m_container->setPalette(palette);
    m_container->setStyleSheet(QStringLiteral("background: transparent;"));
    setWidget(m_container);

    m_scrollAnimation = new QVariantAnimation(this);
    m_scrollAnimation->setDuration(150);
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutQuad);
    connect(m_scrollAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        verticalScrollBar()->setValue(value.toInt());
    });
    bindScrollBarSignals(verticalScrollBar());

    m_followTimer = new QTimer(this);
    m_followTimer->setSingleShot(true);
    m_followTimer->setInterval(16);
    connect(m_followTimer, &QTimer::timeout, this, &MessageListView::executeFollowBottom);
    m_visibleCheckTimer = new QTimer(this);
    m_visibleCheckTimer->setSingleShot(true);
    m_visibleCheckTimer->setInterval(25);
    connect(m_visibleCheckTimer, &QTimer::timeout, this, &MessageListView::checkTopVisibleMessage);
    m_virtualRefreshTimer = new QTimer(this);
    m_virtualRefreshTimer->setSingleShot(true);
    connect(m_virtualRefreshTimer, &QTimer::timeout, this, &MessageListView::updateVisibleCards);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        scheduleVirtualRefresh();
        if (!m_visibleCheckTimer->isActive()) m_visibleCheckTimer->start();
    });
}

void MessageListView::setCustomScrollBar(QScrollBar* scrollBar)
{
    if (m_customScrollBar == scrollBar) return;
    if (m_customScrollBar) m_customScrollBar->disconnect(this);
    m_customScrollBar = scrollBar;
    if (!m_customScrollBar) {
        setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Visible);
        return;
    }
    setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
    QScrollBar* real = verticalScrollBar();
    m_customScrollBar->setRange(real->minimum(), real->maximum());
    m_customScrollBar->setPageStep(real->pageStep());
    m_customScrollBar->setValue(real->value());
    connect(real, &QScrollBar::rangeChanged, m_customScrollBar, [this](int minimum, int maximum) {
        if (!m_customScrollBar) return;
        m_customScrollBar->setRange(minimum, maximum);
        m_customScrollBar->setVisible(maximum > minimum);
    });
    connect(real, &QScrollBar::valueChanged, m_customScrollBar, [this](int value) {
        if (m_customScrollBar && m_customScrollBar->value() != value) m_customScrollBar->setValue(value);
    });
    connect(m_customScrollBar, &QScrollBar::valueChanged, real, [real](int value) {
        if (real->value() != value) real->setValue(value);
    });
    bindScrollBarSignals(m_customScrollBar);
}

void MessageListView::bindScrollBarSignals(QScrollBar* bar)
{
    if (!bar) return;
    connect(bar, &QScrollBar::sliderPressed, this, [this] { m_autoScrollToBottom = false; m_scrollAnimation->stop(); });
    connect(bar, &QScrollBar::sliderReleased, this, [this, bar] {
        if (bar->value() >= bar->maximum() - 10) m_autoScrollToBottom = true;
    });
}

int MessageListView::estimatedHeight(const domain::conversation::Message& message) const
{
    int height = message.role == domain::MessageRole::User ? 84 : 112;
    for (const auto& block : message.blocks) {
        if (block.isThought() || block.isToolCall()) height += 56;
        else if (block.isText()) height += 28;
    }
    return qBound(64, height, 360);
}

int MessageListView::itemIndex(const QUuid& id) const { return m_indexById.value(id, -1); }

void MessageListView::syncMessages(const QList<domain::conversation::Message>& messages)
{
    const int oldScroll = verticalScrollBar()->value();
    const int oldTop = findItemAtY(oldScroll);
    const QUuid anchorId = oldTop >= 0 ? m_items.at(oldTop).message.id : QUuid{};
    const int anchorOffset = oldTop >= 0 ? oldScroll - m_items.at(oldTop).y : 0;
    const bool wasAtBottom = verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 10;

    QSet<QUuid> incoming;
    for (const auto& message : messages) incoming.insert(message.id);
    for (auto it = m_cardMap.begin(); it != m_cardMap.end();) {
        if (!incoming.contains(it.key())) { recycleCard(it.value()); it = m_cardMap.erase(it); }
        else ++it;
    }

    m_items.clear();
    m_indexById.clear();
    m_items.reserve(messages.size());
    for (const auto& message : messages) {
        m_indexById.insert(message.id, m_items.size());
        m_items.push_back({message, 0, m_heightCache.value(message.id, estimatedHeight(message))});
    }
    relayoutItems();
    if (!m_autoScrollToBottom && !anchorId.isNull()) {
        const int index = itemIndex(anchorId);
        if (index >= 0) verticalScrollBar()->setValue(qMax(0, m_items.at(index).y + anchorOffset));
    }
    updateVisibleCards();
    if (wasAtBottom) m_autoScrollToBottom = true;
    scheduleFollowBottom();
}

void MessageListView::relayoutItems()
{
    int y = kTopMargin;
    for (Item& item : m_items) { item.y = y; y += item.height + kItemSpacing; }
    const int totalHeight = qMax(viewport() ? viewport()->height() : 0, y - kItemSpacing + kBottomMargin);
    if (m_container) {
        const int width = qMax(1, viewport() ? viewport()->width() : 0);
        m_container->setMinimumSize(width, totalHeight);
        m_container->resize(width, totalHeight);
    }
}

int MessageListView::findItemAtY(int y) const
{
    int low = 0, high = m_items.size() - 1, found = -1;
    while (low <= high) {
        const int middle = low + (high - low) / 2;
        const Item& item = m_items.at(middle);
        if (item.y + item.height > y) { found = middle; high = middle - 1; }
        else low = middle + 1;
    }
    return found;
}

MessageCardWidget* MessageListView::acquireCard()
{
    MessageCardWidget* card = nullptr;
    if (!m_recycledCards.isEmpty()) {
        card = m_recycledCards.takeLast();
        card->resetForReuse();
    } else {
        card = new MessageCardWidget(m_container);
        connect(card, &MessageCardWidget::contentHeightChanged, this, &MessageListView::onCardHeightChanged);
    }
    return card;
}

void MessageListView::bindCard(MessageCardWidget* card, const Item& item)
{
    card->setAvatarVisible(m_avatarVisible);
    card->setHeaderVisible(m_headerVisible);
    card->syncMessage(item.message);
}

void MessageListView::recycleCard(MessageCardWidget* card)
{
    if (!card) return;
    card->hide();
    m_recycledCards.push_back(card);
}

void MessageListView::updateVisibleCards()
{
    if (!viewport() || m_isRefreshingCards) return;
    m_isRefreshingCards = true;
    const int top = verticalScrollBar()->value();
    const int preload = viewport()->height() * kPreloadViewports;
    const int first = findItemAtY(qMax(0, top - preload));
    const int last = findItemAtY(top + viewport()->height() + preload);
    QSet<QUuid> required;
    const int end = last < 0 ? m_items.size() - 1 : last;
    for (int index = first; index >= 0 && index <= end; ++index) required.insert(m_items.at(index).message.id);
    for (auto it = m_cardMap.begin(); it != m_cardMap.end();) {
        if (!required.contains(it.key())) { recycleCard(it.value()); it = m_cardMap.erase(it); }
        else ++it;
    }

    bool heightsChanged = false;
    const int availableWidth = qMax(1, m_container->width() - 2 * kHorizontalMargin);
    const int width = qMin(kMessageContentMaxWidth, availableWidth);
    const int x = qMax(kHorizontalMargin, (m_container->width() - width) / 2);
    for (int index = first; index >= 0 && index <= end; ++index) {
        Item& item = m_items[index];
        MessageCardWidget* card = m_cardMap.value(item.message.id);
        if (!card) { card = acquireCard(); m_cardMap.insert(item.message.id, card); }
        // Width must be known before syncMessage() constructs Markdown layouts;
        // otherwise a pooled card can measure at 1px and poison the height cache.
        card->setGeometry(x, item.y, width, qMax(item.height, 1));
        bindCard(card, item);
        const int measured = qMax(1, card->sizeHint().height());
        if (measured != item.height) { item.height = measured; m_heightCache.insert(item.message.id, measured); heightsChanged = true; }
    }
    if (heightsChanged) relayoutItems();
    for (auto it = m_cardMap.cbegin(); it != m_cardMap.cend(); ++it) {
        const int index = itemIndex(it.key());
        if (index < 0) continue;
        const Item& item = m_items.at(index);
        it.value()->setGeometry(x, item.y, width, item.height);
        it.value()->show();
    }
    m_isRefreshingCards = false;
}

void MessageListView::onCardHeightChanged()
{
    if (m_isRefreshingCards) return;
    auto* card = qobject_cast<MessageCardWidget*>(sender());
    if (!card) return;
    if (m_cardMap.value(card->messageId()) != card) return;
    const int index = itemIndex(card->messageId());
    if (index < 0) return;
    Item& item = m_items[index];
    const int height = qMax(1, card->sizeHint().height());
    if (item.height == height) return;
    const bool aboveViewport = item.y + item.height <= verticalScrollBar()->value();
    const int delta = height - item.height;
    item.height = height;
    m_heightCache.insert(item.message.id, height);
    relayoutItems();
    if (!m_autoScrollToBottom && aboveViewport) verticalScrollBar()->setValue(verticalScrollBar()->value() + delta);
    scheduleVirtualRefresh();
    scheduleFollowBottom();
}

void MessageListView::scheduleVirtualRefresh()
{
    if (m_virtualRefreshTimer && !m_virtualRefreshTimer->isActive()) m_virtualRefreshTimer->start();
}

void MessageListView::setAvatarVisible(bool visible)
{
    if (m_avatarVisible == visible) return;
    m_avatarVisible = visible;
    invalidateMeasuredHeights();
}

void MessageListView::setHeaderVisible(bool visible)
{
    if (m_headerVisible == visible) return;
    m_headerVisible = visible;
    invalidateMeasuredHeights();
}

void MessageListView::invalidateMeasuredHeights()
{
    m_heightCache.clear();
    for (Item& item : m_items) item.height = estimatedHeight(item.message);
    relayoutItems();
    updateVisibleCards();
}

void MessageListView::clear()
{
    for (MessageCardWidget* card : m_cardMap) card->deleteLater();
    for (MessageCardWidget* card : m_recycledCards) card->deleteLater();
    m_cardMap.clear(); m_recycledCards.clear(); m_items.clear(); m_indexById.clear(); m_heightCache.clear();
    relayoutItems();
}

void MessageListView::scrollToBottom() { m_autoScrollToBottom = true; scheduleFollowBottom(); }

void MessageListView::scrollToMessage(const QUuid& id)
{
    const int index = itemIndex(id);
    if (index < 0) return;
    m_autoScrollToBottom = false;
    m_scrollAnimation->stop();
    verticalScrollBar()->setValue(qBound(0, m_items.at(index).y - kTopMargin, verticalScrollBar()->maximum()));
}

void MessageListView::scrollToMessage(const QString& idString) { scrollToMessage(QUuid::fromString(idString)); }

void MessageListView::checkTopVisibleMessage()
{
    const int index = findItemAtY(verticalScrollBar()->value() + 10);
    if (index < 0) return;
    const QUuid id = m_items.at(index).message.id;
    if (id != m_lastTopVisibleId) { m_lastTopVisibleId = id; emit topVisibleMessageChanged(id); }
}

void MessageListView::scheduleFollowBottom()
{
    if (m_autoScrollToBottom && !m_followTimer->isActive()) m_followTimer->start();
}

void MessageListView::executeFollowBottom()
{
    QScrollBar* bar = verticalScrollBar();
    if (bar->value() >= bar->maximum()) return;
    m_scrollAnimation->stop();
    m_scrollAnimation->setDuration(150);
    m_scrollAnimation->setStartValue(bar->value());
    m_scrollAnimation->setEndValue(bar->maximum());
    m_scrollAnimation->start();
}

void MessageListView::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0) { m_autoScrollToBottom = false; m_scrollAnimation->stop(); }
    fluent::scrolling::ScrollView::wheelEvent(event);
    if (verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 10) m_autoScrollToBottom = true;
}

void MessageListView::resizeEvent(QResizeEvent* event)
{
    fluent::scrolling::ScrollView::resizeEvent(event);
    relayoutItems();
    updateVisibleCards();
}

void MessageListView::showEvent(QShowEvent* event)
{
    fluent::scrolling::ScrollView::showEvent(event);
    relayoutItems();
    updateVisibleCards();
    if (m_autoScrollToBottom) QTimer::singleShot(0, this, &MessageListView::executeFollowBottom);
}

void MessageListView::onThemeUpdated()
{
    fluent::scrolling::ScrollView::onThemeUpdated();
    if (viewport()) {
        QPalette palette = viewport()->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        viewport()->setPalette(palette);
        viewport()->update();
    }
    if (m_container) {
        QPalette palette = m_container->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        m_container->setPalette(palette);
        m_container->update();
    }
}

} // namespace ui::widget::message
