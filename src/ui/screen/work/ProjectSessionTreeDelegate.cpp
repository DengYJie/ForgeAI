#include "ProjectSessionTreeDelegate.h"
#include <QAbstractItemView>
#include <QMouseEvent>
#include <QPainter>

namespace ui::screen::work {
namespace {
constexpr int kProjectHeight = 44;
constexpr int kConversationHeight = 46;
constexpr int kActionSize = 24;
constexpr int kActionMargin = 6;
constexpr int kItemKindRole = Qt::UserRole + 3;
constexpr int kPinnedRole = Qt::UserRole + 4;
}

ProjectSessionTreeDelegate::ProjectSessionTreeDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize ProjectSessionTreeDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const {
    return {0, index.data(kItemKindRole).toInt() == ProjectItem ? kProjectHeight : kConversationHeight};
}

void ProjectSessionTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (!index.isValid()) return;
    const bool project = index.data(kItemKindRole).toInt() == ProjectItem;
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const bool pinned = index.data(kPinnedRole).toBool();
    const auto& colors = themeColorsRef();
    const QRect rect = option.rect.adjusted(4, 1, -4, -1);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (selected || hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? colors.subtleSecondary : colors.subtleTertiary);
        painter->drawRoundedRect(rect, 12, 12);
    }
    const int left = project ? rect.left() + 12 : rect.left() + 44;
    const int iconWidth = project ? 24 : 0;
    if (project) {
        painter->setPen(selected ? colors.textPrimary : colors.textSecondary);
        Typography::Icons::paintGlyph(*painter, QRect(left, rect.top(), iconWidth, rect.height()),
                                      Typography::Icons::Folder, 17, Qt::AlignVCenter);
    }
    const bool showActions = !project && hovered;
    const int actionWidth = project && hovered ? 60 : (showActions ? kActionSize * 2 + kActionMargin * 2 : 0);
    const QRect textRect(left + iconWidth, rect.top(), rect.right() - (left + iconWidth) - actionWidth, rect.height());
    QFont font = themeFont(Typography::FontRole::Body).toQFont();
    font.setPixelSize(Typography::FontSize::Caption);
    painter->setFont(font);
    painter->setPen(selected ? colors.textPrimary : colors.textSecondary);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, textRect.width()));

    if (project && hovered) {
        const QRect moreRect(rect.right() - 56, rect.top() + 10, 20, 20);
        const QRect editRect = projectEditButtonRect(option.rect);
        Typography::Icons::paintGlyph(*painter, moreRect, Typography::Icons::More, 13, Qt::AlignCenter);
        Typography::Icons::paintGlyph(*painter, editRect, Typography::Icons::Edit, 13, Qt::AlignCenter);
    } else if (showActions) {
        const QRect archiveRect = archiveButtonRect(option.rect);
        const QRect pinRect = pinButtonRect(option.rect);
        if (archiveRect.contains(m_hoveredPos) || pinRect.contains(m_hoveredPos)) {
            const QRect hit = archiveRect.contains(m_hoveredPos) ? archiveRect : pinRect;
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.subtleSecondary);
            painter->drawRoundedRect(hit, 4, 4);
        }
        painter->setPen(pinned ? colors.accentDefault : colors.textSecondary);
        painter->setFont(QFont(Typography::FontFamily::FluentIcons, 10));
        painter->drawText(pinRect, Qt::AlignCenter, pinned ? Typography::Icons::PinFill : Typography::Icons::Pin);
        painter->setPen(colors.textSecondary);
        painter->drawText(archiveRect, Qt::AlignCenter, QString(QChar(0xE7B8)));
    }
    painter->restore();
}

QRect ProjectSessionTreeDelegate::pinButtonRect(const QRect& itemRect) const {
    return {itemRect.right() - kActionSize - kActionMargin, itemRect.top() + (itemRect.height() - kActionSize) / 2,
            kActionSize, kActionSize};
}

QRect ProjectSessionTreeDelegate::archiveButtonRect(const QRect& itemRect) const {
    return {itemRect.right() - kActionSize * 2 - kActionMargin * 2, itemRect.top() + (itemRect.height() - kActionSize) / 2,
            kActionSize, kActionSize};
}

QRect ProjectSessionTreeDelegate::projectEditButtonRect(const QRect& itemRect) const {
    return {itemRect.right() - 34, itemRect.top() + (itemRect.height() - 20) / 2, 20, 20};
}

bool ProjectSessionTreeDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                              const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (!index.isValid()) {
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
    if (event->type() == QEvent::MouseMove) {
        m_hoveredPos = static_cast<QMouseEvent*>(event)->pos();
        if (auto* view = qobject_cast<QAbstractItemView*>(parent())) view->viewport()->update(option.rect);
        return false;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (index.data(kItemKindRole).toInt() == ProjectItem) {
                if (projectEditButtonRect(option.rect).contains(mouseEvent->pos())) {
                    emit newConversationRequested(index.data(Qt::UserRole + 2).toUuid());
                    return true;
                }
                return QStyledItemDelegate::editorEvent(event, model, option, index);
            }
            const QString id = index.data(Qt::UserRole + 1).toString();
            if (pinButtonRect(option.rect).contains(mouseEvent->pos())) {
                emit pinClicked(id, !index.data(kPinnedRole).toBool());
                return true;
            }
            if (archiveButtonRect(option.rect).contains(mouseEvent->pos())) {
                emit archiveClicked(id);
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
}
