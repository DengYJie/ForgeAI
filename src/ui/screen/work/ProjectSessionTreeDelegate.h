#pragma once

#include <QStyledItemDelegate>
#include <QUuid>
#include <QTimer>
#include <FluentQt/Foundation.h>
#include <FluentQt/Design.h>

namespace ui::screen::work {
class ProjectSessionTreeDelegate final : public QStyledItemDelegate, public fluent::FluentElement {
    Q_OBJECT
public:
    enum ItemKind { ProjectItem = 1, ConversationItem = 2, ShowMoreItem = 3 };
    explicit ProjectSessionTreeDelegate(QObject* parent = nullptr);
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

    void onThemeUpdated() override {}

Q_SIGNALS:
    void newConversationRequested(const QUuid& projectId);
    void projectMoreRequested(const QUuid& projectId, const QPoint& globalPos);
    void pinClicked(const QString& id, bool pinned);
    void archiveClicked(const QString& id);

private:
    QRect pinButtonRect(const QRect& itemRect) const;
    QRect archiveButtonRect(const QRect& itemRect) const;
    QRect projectMoreButtonRect(const QRect& itemRect) const;
    QRect projectEditButtonRect(const QRect& itemRect) const;
    QPoint m_hoveredPos = QPoint(-1, -1);
    QTimer m_animationTimer;
};
}
