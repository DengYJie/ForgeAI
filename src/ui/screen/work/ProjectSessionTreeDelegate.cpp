#include "ProjectSessionTreeDelegate.h"
#include <QAbstractItemView>
#include <QMouseEvent>
#include <QPainter>
#include <QDateTime>
#include "ui/widget/chat/ConversationRowActions.h"

namespace ui::screen::work {
namespace {
constexpr int kProjectHeight = 34;
constexpr int kConversationHeight = 32;
constexpr int kActionSize = 20;
constexpr int kActionMargin = 4;
constexpr int kItemKindRole = Qt::UserRole + 3;
constexpr int kPinnedRole = Qt::UserRole + 4;
}

ProjectSessionTreeDelegate::ProjectSessionTreeDelegate(QObject* parent) : QStyledItemDelegate(parent) {
    m_animationTimer.setInterval(30);
    connect(&m_animationTimer, &QTimer::timeout, this, [this] {
        if (auto* view = qobject_cast<QAbstractItemView*>(this->parent())) {
            if (auto* model = view->model()) {
                bool hasProcessing = false;
                for (int i = 0; i < model->rowCount(); ++i) {
                    auto projIndex = model->index(i, 0);
                    for (int j = 0; j < model->rowCount(projIndex); ++j) {
                        if (model->index(j, 0, projIndex).data(Qt::UserRole + 5).toBool()) {
                            hasProcessing = true; break;
                        }
                    }
                    if (hasProcessing) break;
                }
                if (hasProcessing) view->viewport()->update();
            }
        }
    });
    m_animationTimer.start();
}

QSize ProjectSessionTreeDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const {
    return {0, index.data(kItemKindRole).toInt() == ProjectItem ? kProjectHeight : kConversationHeight};
}

void ProjectSessionTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (!index.isValid()) return;
    const int kind = index.data(kItemKindRole).toInt();
    const bool project = (kind == ProjectItem);
    const bool showMore = (kind == ShowMoreItem);
    const bool selected = option.state.testFlag(QStyle::State_Selected) && !showMore;
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const bool pinned = index.data(kPinnedRole).toBool();
    const bool processing = index.data(Qt::UserRole + 5).toBool();
    const auto& colors = themeColorsRef();
    const QRect bgRect = option.rect.adjusted(2, 1, -2, -1);
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    
    if (selected || hovered) {
        painter->setPen(Qt::NoPen);
        if (showMore) {
            painter->setBrush(colors.subtleTertiary);
        } else {
            painter->setBrush(selected ? colors.subtleSecondary : colors.subtleTertiary);
        }
        painter->drawRoundedRect(bgRect, 4, 4);
    }
    
    const int left = project ? bgRect.left() + 6 : bgRect.left() + 28;
    const int iconWidth = project ? 22 : 0;
    if (project) {
        painter->setPen(selected ? colors.textPrimary : colors.textSecondary);
        const bool isExpanded = option.state.testFlag(QStyle::State_Open);
        const QString glyph = isExpanded ? Typography::Icons::glyph(QStringLiteral("ic_fluent_folder_open_20_regular")) : Typography::Icons::glyph(QStringLiteral("ic_fluent_folder_16_regular"));
        Typography::Icons::paintGlyph(*painter, QRect(left, bgRect.top(), iconWidth, bgRect.height()),
                                      glyph, 14, Qt::AlignVCenter);
    }
    
    const bool showActions = (!project && !showMore && hovered) && !processing;
    const int processingWidth = processing ? 32 : 0;
    const int actionWidth = (project && hovered) ? 60 : (showActions ? kActionSize * 2 + kActionMargin * 2 : processingWidth);
    
    const QRect textRect(left + iconWidth, bgRect.top(), bgRect.right() - (left + iconWidth) - actionWidth, bgRect.height());
    QFont font = themeFont(Typography::FontRole::Body).toQFont();
    font.setPixelSize(Typography::FontSize::Caption);
    painter->setFont(font);
    
    if (showMore) {
        painter->setPen(colors.textSecondary);
    } else {
        painter->setPen(selected ? colors.textPrimary : colors.textSecondary);
    }
    
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, textRect.width()));

    if (processing) {
        const qint64 msecs = QDateTime::currentMSecsSinceEpoch() % 1000;
        const int startAngle = static_cast<int>((msecs * 360 / 1000) * 16);
        const int spanAngle = 270 * 16;
        QRect arcRect(bgRect.right() - 28, bgRect.top() + (bgRect.height() - 16) / 2, 16, 16);
        QPen arcPen(colors.textSecondary, 1.5, Qt::SolidLine, Qt::RoundCap);
        painter->setPen(arcPen);
        painter->drawArc(arcRect, -startAngle, -spanAngle);
    } else if (project && hovered) {
        const QRect moreRect = projectMoreButtonRect(option.rect);
        const QRect editRect = projectEditButtonRect(option.rect);
        if (moreRect.contains(m_hoveredPos)) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.subtleSecondary);
            painter->drawRoundedRect(moreRect, 4, 4);
        } else if (editRect.contains(m_hoveredPos)) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.subtleSecondary);
            painter->drawRoundedRect(editRect, 4, 4);
        }
        painter->setPen(colors.textSecondary);
        Typography::Icons::paintGlyph(*painter, moreRect, Typography::Icons::More, 13, Qt::AlignCenter);
        Typography::Icons::paintGlyph(*painter, editRect, Typography::Icons::Edit, 13, Qt::AlignCenter);
    } else {
        widget::chat::ConversationRowActions::paint(painter, option, m_hoveredPos, showActions, showActions, pinned);
    }
    painter->restore();
}

QRect ProjectSessionTreeDelegate::projectMoreButtonRect(const QRect& itemRect) const {
    const int y = itemRect.top() + (itemRect.height() - 20) / 2;
    const int x = itemRect.right() - 52;
    return QRect(x, y, 20, 20);
}

QRect ProjectSessionTreeDelegate::projectEditButtonRect(const QRect& itemRect) const {
    const int y = itemRect.top() + (itemRect.height() - 20) / 2;
    const int x = itemRect.right() - 28;
    return QRect(x, y, 20, 20);
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
                const auto projId = index.data(Qt::UserRole + 2).toUuid();
                if (projectMoreButtonRect(option.rect).contains(mouseEvent->pos())) {
                    emit projectMoreRequested(projId, mouseEvent->globalPosition().toPoint());
                    return true;
                }
                if (projectEditButtonRect(option.rect).contains(mouseEvent->pos())) {
                    emit newConversationRequested(projId);
                    return true;
                }
                return QStyledItemDelegate::editorEvent(event, model, option, index);
            }
            const QString id = index.data(Qt::UserRole + 1).toString();
            const auto hit = widget::chat::ConversationRowActions::hitTest(option.rect, mouseEvent->pos());
            if (hit == widget::chat::ConversationRowActions::HitTarget::Pin) {
                emit pinClicked(id, !index.data(kPinnedRole).toBool());
                return true;
            }
            if (hit == widget::chat::ConversationRowActions::HitTarget::Archive) {
                emit archiveClicked(id);
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
}

