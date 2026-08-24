#pragma once

#include <QHash>
#include <QScrollBar>
#include <QTimer>
#include <QUuid>
#include <QVariantAnimation>
#include <QVector>

#include "domain/conversation/Message.h"
#include <FluentQt/Scrolling.h>

namespace ui::widget::message {

class MessageCardWidget;

/**
 * A Qt Widgets virtual message list.
 *
 * Items retain only message data and a measured/estimated height.  Card
 * widgets are owned by a small visible window plus preload area and returned
 * to an object pool once they leave it.
 */
class MessageListView : public fluent::scrolling::ScrollView {
    Q_OBJECT
public:
    explicit MessageListView(QWidget* parent = nullptr);
    ~MessageListView() override;

    void syncMessages(const QList<domain::conversation::Message>& messages);
    void clear();
    void setCustomScrollBar(QScrollBar* scrollBar);
    void scrollToBottom();
    void scrollToMessage(const QUuid& id);
    void scrollToMessage(const QString& idString);
    void setAvatarVisible(bool visible);
    bool isAvatarVisible() const { return m_avatarVisible; }
    void setHeaderVisible(bool visible);
    bool isHeaderVisible() const { return m_headerVisible; }

    int messageCount() const noexcept { return m_items.size(); }
    int activeCardCount() const noexcept { return m_cardMap.size(); }
    int pooledCardCount() const noexcept { return m_recycledCards.size(); }

Q_SIGNALS:
    void topVisibleMessageChanged(const QUuid& id);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void onThemeUpdated() override;

private slots:
    void executeFollowBottom();
    void scheduleFollowBottom();
    void onCardHeightChanged();
    void checkTopVisibleMessage();
    void scheduleVirtualRefresh();

private:
    struct Item {
        domain::conversation::Message message;
        int y = 0;
        int height = 0;
    };

    void setupUi();
    void bindScrollBarSignals(QScrollBar* bar);
    void relayoutItems();
    void updateVisibleCards();
    void recycleCard(MessageCardWidget* card);
    MessageCardWidget* acquireCard();
    void bindCard(MessageCardWidget* card, const Item& item);
    int findItemAtY(int y) const;
    int estimatedHeight(const domain::conversation::Message& message) const;
    int itemIndex(const QUuid& id) const;
    void invalidateMeasuredHeights();

    QWidget* m_container = nullptr;
    QVector<Item> m_items;
    QHash<QUuid, int> m_indexById;
    QHash<QUuid, int> m_heightCache;
    QHash<QUuid, MessageCardWidget*> m_cardMap;
    QVector<MessageCardWidget*> m_recycledCards;

    QScrollBar* m_customScrollBar = nullptr;
    QTimer* m_followTimer = nullptr;
    QTimer* m_visibleCheckTimer = nullptr;
    QTimer* m_virtualRefreshTimer = nullptr;
    QVariantAnimation* m_scrollAnimation = nullptr;
    bool m_autoScrollToBottom = true;
    bool m_isRefreshingCards = false;
    QUuid m_lastTopVisibleId;
    bool m_avatarVisible = true;
    bool m_headerVisible = true;
};

} // namespace ui::widget::message
