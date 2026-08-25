#include "MarkdownViewEventFilter.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

namespace ui::widget {

MarkdownViewEventFilter::MarkdownViewEventFilter(QWidget* viewport, QObject* parent)
    : QObject(parent)
    , m_viewport(viewport)
{}

void MarkdownViewEventFilter::setDocumentLayout(const ui::markdown::DocumentLayout* layout)
{
    m_layout = layout;
}

void MarkdownViewEventFilter::setScrollOffsets(const QHash<int, ui::markdown::BlockScrollOffset>* offsets)
{
    m_scrollOffsets = offsets;
}

void MarkdownViewEventFilter::setScrollBarValueGetter(std::function<int()> getter)
{
    m_scrollBarValueGetter = std::move(getter);
}

void MarkdownViewEventFilter::setSelectable(bool selectable)
{
    m_selectable = selectable;
    if (!m_selectable) {
        m_selection = {};
        m_selectionAnchor = -1;
        emit selectionChanged(false);
        emit repaintRequested();
    }
}

void MarkdownViewEventFilter::setTaskListInteractive(bool interactive)
{
    m_taskListInteractive = interactive;
}

ui::markdown::TextSelection MarkdownViewEventFilter::selection() const
{
    return m_selection;
}

void MarkdownViewEventFilter::setSelection(const ui::markdown::TextSelection& sel)
{
    m_selection = sel;
}

void MarkdownViewEventFilter::clearSelection()
{
    m_selection = {};
    m_selectionAnchor = -1;
    emit selectionChanged(false);
    emit repaintRequested();
}

int MarkdownViewEventFilter::hoveredBlock() const { return m_hoveredBlock; }
int MarkdownViewEventFilter::hoveredCopyBlock() const { return m_hoveredCopyBlock; }
int MarkdownViewEventFilter::copiedBlock() const { return m_copiedBlock; }

QPointF MarkdownViewEventFilter::toDocument(const QPointF& viewportPos, int scrollBarValue) const
{
    const int sv = m_scrollBarValueGetter ? m_scrollBarValueGetter() : scrollBarValue;
    return viewportPos + QPointF(0, sv);
}

bool MarkdownViewEventFilter::handleViewportEvent(QEvent* event)
{
    if (!m_layout) return false;

    static const QHash<int, ui::markdown::BlockScrollOffset> emptyOffsets;
    const auto& offsets = m_scrollOffsets ? *m_scrollOffsets : emptyOffsets;

    if (event->type() == QEvent::Leave) {
        m_hoveredBlock = -1;
        m_hoveredCopyBlock = -1;
        emit cursorChanged(Qt::ArrowCursor);
        emit repaintRequested();
        return true;
    }

    if (event->type() == QEvent::MouseMove) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPointF docPos = toDocument(mouse->position(), 0);
        const auto hit = m_renderer.hitTest(*m_layout, docPos, offsets);
        const int prevHovered = m_hoveredBlock;
        const int prevCopy = m_hoveredCopyBlock;
        m_hoveredBlock = hit.blockIndex;
        m_hoveredCopyBlock = (hit.kind == ui::markdown::HitKind::CodeCopy) ? hit.blockIndex : -1;
        Qt::CursorShape cursor = Qt::ArrowCursor;
        if (hit.kind == ui::markdown::HitKind::Link || hit.kind == ui::markdown::HitKind::CodeCopy) {
            cursor = Qt::PointingHandCursor;
        } else if (hit.kind == ui::markdown::HitKind::TaskCheckbox && m_taskListInteractive) {
            cursor = Qt::PointingHandCursor;
        } else if (hit.kind == ui::markdown::HitKind::Text) {
            cursor = m_selectable ? Qt::IBeamCursor : Qt::ArrowCursor;
        }
        emit cursorChanged(cursor);
        if (m_selecting && hit.textOffset >= 0) setSelectionPosition(hit.textOffset, true);
        if (hit.kind == ui::markdown::HitKind::Link) emit linkHighlighted(QUrl(hit.value));
        if (m_hoveredBlock != prevHovered || m_hoveredCopyBlock != prevCopy) emit repaintRequested();
        else if (m_selecting) emit repaintRequested();
        return true;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            const auto hit = m_renderer.hitTest(*m_layout, toDocument(mouse->position(), 0), offsets);
            m_selecting = m_selectable && (hit.kind == ui::markdown::HitKind::Text || hit.kind == ui::markdown::HitKind::Link);
            if (m_selecting) setSelectionPosition(hit.textOffset, mouse->modifiers().testFlag(Qt::ShiftModifier));
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            const auto hit = m_renderer.hitTest(*m_layout, toDocument(mouse->position(), 0), offsets);
            if (m_selecting && m_selection.anchor == m_selection.position && hit.kind == ui::markdown::HitKind::Link)
                emit linkActivated(QUrl(hit.value));
            if (hit.kind == ui::markdown::HitKind::Image)
                emit imageActivated(QUrl(hit.value));
            if (hit.kind == ui::markdown::HitKind::CodeCopy) {
                QGuiApplication::clipboard()->setText(hit.value);
                emit copyCodeRequested(hit.value, hit.blockIndex);
                showCopiedFeedback(hit.blockIndex);
            }
            if (hit.kind == ui::markdown::HitKind::TaskCheckbox && m_taskListInteractive && hit.blockIndex >= 0)
                emit taskToggleRequested(hit.blockIndex);
            m_selecting = false;
            return true;
        }
    }

    if (event->type() == QEvent::ContextMenu) {
        auto* context = static_cast<QContextMenuEvent*>(event);
        const auto hit = m_renderer.hitTest(*m_layout, toDocument(context->pos(), 0), offsets);
        emit contextMenuRequested(context->pos(),
                                  hit.kind == ui::markdown::HitKind::Link ? QUrl(hit.value) : QUrl{},
                                  hit.kind == ui::markdown::HitKind::Image ? QUrl(hit.value) : QUrl{});
        QMenu menu(m_viewport);
        if (hit.kind == ui::markdown::HitKind::Link) {
            QAction* open = menu.addAction(QObject::tr("打开链接"));
            const QUrl url(hit.value);
            connect(open, &QAction::triggered, this, [this, url] { emit linkActivated(url); });
            QAction* copyLink = menu.addAction(QObject::tr("复制链接"));
            connect(copyLink, &QAction::triggered, this, [url] { QGuiApplication::clipboard()->setText(url.toString()); });
        } else if (hit.kind == ui::markdown::HitKind::CodeCopy ||
                   (hit.blockIndex >= 0 && m_layout->blocks.at(hit.blockIndex).kind == ui::markdown::BlockKind::CodeBlock)) {
            const QString code = hit.kind == ui::markdown::HitKind::CodeCopy
                ? hit.value
                : m_layout->blocks.at(hit.blockIndex).code;
            QAction* copyCode = menu.addAction(QObject::tr("复制代码"));
            const int idx = hit.blockIndex;
            connect(copyCode, &QAction::triggered, this, [this, code, idx] {
                QGuiApplication::clipboard()->setText(code);
                showCopiedFeedback(idx);
            });
        } else {
            QAction* copyText = menu.addAction(QObject::tr("复制"));
            copyText->setEnabled(m_selection.isValid());
            connect(copyText, &QAction::triggered, this, [this] {
                emit copyCodeRequested({}, -1);
            });
            QAction* selectAll = menu.addAction(QObject::tr("全选"));
            connect(selectAll, &QAction::triggered, this, [this] {
                emit taskToggleRequested(-2);
            });
        }
        menu.exec(context->globalPos());
        return true;
    }

    return false;
}

void MarkdownViewEventFilter::setSelectionPosition(int position, bool extend)
{
    if (position < 0) return;
    if (!extend || m_selectionAnchor < 0) m_selectionAnchor = position;
    m_selection = {m_selectionAnchor, position};
    emit selectionChanged(m_selection.isValid());
    emit repaintRequested();
}

void MarkdownViewEventFilter::showCopiedFeedback(int blockIndex)
{
    m_copiedBlock = blockIndex;
    emit repaintRequested();
    QTimer::singleShot(1200, this, [this, blockIndex] {
        if (m_copiedBlock != blockIndex) return;
        m_copiedBlock = -1;
        emit repaintRequested();
    });
}

} // namespace ui::widget
