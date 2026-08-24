#include "ChatSessionDelegate.h"
#include "ChatSessionListModel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QAbstractItemView>

namespace ui::screen::chat {
    namespace {
        constexpr int kItemHeight = 32;
        constexpr int kButtonSize = 20;
        constexpr int kButtonMargin = 4;
    } // namespace

    ChatSessionDelegate::ChatSessionDelegate(QObject *parent)
        : QStyledItemDelegate(parent) {
    }

    QRect ChatSessionDelegate::pinButtonRect(const QRect &itemRect) const {
        const int y = itemRect.top() + (itemRect.height() - kButtonSize) / 2;
        const int x = itemRect.right() - kButtonSize - kButtonMargin;
        return QRect(x, y, kButtonSize, kButtonSize);
    }

    QRect ChatSessionDelegate::archiveButtonRect(const QRect &itemRect) const {
        const int y = itemRect.top() + (itemRect.height() - kButtonSize) / 2;
        const int x = itemRect.right() - (kButtonSize * 2) - (kButtonMargin * 2);
        return QRect(x, y, kButtonSize, kButtonSize);
    }

    QSize ChatSessionDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(0, kItemHeight);
    }

    void ChatSessionDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
        if (!index.isValid())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const auto &colors = themeColorsRef();
        const bool isSelected = (option.state & QStyle::State_Selected);
        const bool isHovered = (option.state & QStyle::State_MouseOver);
        const bool isPinned = index.data(ChatSessionListModel::IsPinnedRole).toBool();
        const QString title = index.data(ChatSessionListModel::TitleRole).toString();
        const bool isEmptyDraft = title == tr("新对话");

        // 1. 绘制胶囊背景（精细圆角）
        const QRect bgRect = option.rect.adjusted(2, 1, -2, -1);
        if (isSelected) {
            painter->setBrush(colors.subtleSecondary);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(bgRect, 4, 4);
        } else if (isHovered) {
            painter->setBrush(colors.subtleTertiary);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(bgRect, 4, 4);
        }

        // 2. 绘制标题（12.5px / Caption 精致排版）
        const bool showButtons = !isEmptyDraft && (isHovered || isSelected);
        const int buttonsTotalWidth = (showButtons || isPinned) ? 52 : 6;
        const QRect textRect(option.rect.left() + 10, option.rect.top(),
                             option.rect.width() - 10 - buttonsTotalWidth, option.rect.height());

        QFont itemFont = themeFont(Typography::FontRole::Body).toQFont();
        itemFont.setPixelSize(Typography::FontSize::Caption); // 12px 紧凑正文
        painter->setFont(itemFont);
        painter->setPen(isSelected ? colors.textPrimary : colors.textSecondary);
        const QString elidedTitle = painter->fontMetrics().elidedText(title, Qt::ElideRight, textRect.width());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedTitle);

        // 3. 绘制右侧操作按钮（精致小图标）
        const QRect pinRect = pinButtonRect(option.rect);
        const QRect archiveRect = archiveButtonRect(option.rect);

        if (!isEmptyDraft && (showButtons || isPinned)) {
            // Pin 按钮
            if (pinRect.contains(m_hoveredPos)) {
                painter->setBrush(colors.subtleSecondary);
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(pinRect, 3, 3);
            }

            painter->setFont(QFont(Typography::FontFamily::FluentIcons, 10));
            painter->setPen(isPinned ? colors.accentDefault : colors.textSecondary);
            painter->drawText(pinRect, Qt::AlignCenter, isPinned ? Typography::Icons::PinFill : Typography::Icons::Pin);
        }

        if (showButtons) {
            if (archiveRect.contains(m_hoveredPos)) {
                painter->setBrush(colors.subtleSecondary);
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(archiveRect, 3, 3);
            }
            painter->setPen(colors.textSecondary);
            painter->setFont(QFont(Typography::FontFamily::FluentIcons, 10));
            // Fluent UI System Icons archive glyph (Segoe Fluent Icons compatible).
            painter->drawText(archiveRect, Qt::AlignCenter, QString(QChar(0xE7B8)));

        }

        painter->restore();
    }

    bool ChatSessionDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) {
        if (!index.isValid())
            return false;

        const QString id = index.data(ChatSessionListModel::IdRole).toString();
        const bool isPinned = index.data(ChatSessionListModel::IsPinnedRole).toBool();
        const bool isEmptyDraft = index.data(ChatSessionListModel::TitleRole).toString() == tr("新对话");

        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            m_hoveredPos = me->pos();
            if (auto *view = qobject_cast<QAbstractItemView *>(parent())) {
                view->viewport()->update(option.rect);
            }
            return false;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                if (isEmptyDraft) return QStyledItemDelegate::editorEvent(event, model, option, index);
                const QRect pinRect = pinButtonRect(option.rect);
                const QRect archiveRect = archiveButtonRect(option.rect);

                if (pinRect.contains(me->pos())) {
                    emit pinClicked(id, !isPinned);
                    return true;
                }

                if (archiveRect.contains(me->pos())) {
                    emit archiveClicked(id);
                    return true;
                }

            }
        }

        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
} // namespace ui::screen::chat
