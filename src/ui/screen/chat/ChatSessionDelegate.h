#pragma once

#include <QStyledItemDelegate>
#include <FluentQt/Foundation.h>
#include <FluentQt/Design.h>

namespace ui::screen::chat {
    class ChatSessionDelegate : public QStyledItemDelegate, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ChatSessionDelegate(QObject *parent = nullptr);

        ~ChatSessionDelegate() override = default;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

        bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                         const QModelIndex &index) override;

        void onThemeUpdated() override {
        }

        Q_SIGNALS:




        void pinClicked(const QString &id, bool pinned);

        void archiveClicked(const QString &id);

    private:
        QPoint m_hoveredPos = QPoint(-1, -1);
    };
} // namespace ui::screen::chat
