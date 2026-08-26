#pragma once

#include "ui/markdown/MarkdownLayout.h"
#include "ui/markdown/MarkdownRenderer.h"

#include <QObject>
#include <QPoint>
#include <QUrl>
#include <functional>

class QEvent;
class QWidget;

namespace ui::widget {

class MarkdownViewEventFilter : public QObject
{
    Q_OBJECT

public:
    explicit MarkdownViewEventFilter(QWidget* viewport, QObject* parent = nullptr);

    void setDocumentLayout(const ui::markdown::DocumentLayout* layout);
    void setScrollOffsets(const QHash<int, ui::markdown::BlockScrollOffset>* offsets);
    void setScrollBarValueGetter(std::function<int()> getter);
    void setSelectable(bool selectable);
    void setTaskListInteractive(bool interactive);

    bool handleViewportEvent(QEvent* event);

    ui::markdown::TextSelection selection() const;
    void setSelection(const ui::markdown::TextSelection& sel);
    void clearSelection();
    bool isSelectable() const;
    bool isTaskListInteractive() const;
    int hoveredBlock() const;
    int hoveredCopyBlock() const;
    int copiedBlock() const;

    QPointF toDocument(const QPointF& viewportPos, int scrollBarValue) const;

signals:
    void repaintRequested();
    void linkActivated(const QUrl& url);
    void linkHighlighted(const QUrl& url);
    void selectionChanged(bool hasSelection);
    void imageActivated(const QUrl& url);
    void contextMenuRequested(const QPoint& pos, const QUrl& linkUrl, const QUrl& imageUrl);
    void copyCodeRequested(const QString& code, int blockIndex);
    void copySelectionRequested();
    void selectAllRequested();
    void taskToggleRequested(int blockIndex);
    void blockScrollRequested(int blockIndex, qreal dx, qreal dy, bool smooth);
    void cursorChanged(Qt::CursorShape shape);

private:
    void setSelectionPosition(int position, bool extend);
    void showCopiedFeedback(int blockIndex);

    QWidget* m_viewport = nullptr;
    const ui::markdown::DocumentLayout* m_layout = nullptr;
    const QHash<int, ui::markdown::BlockScrollOffset>* m_scrollOffsets = nullptr;
    std::function<int()> m_scrollBarValueGetter;
    ui::markdown::MarkdownRenderer m_renderer;
    ui::markdown::TextSelection m_selection;
    int m_selectionAnchor = -1;
    int m_hoveredBlock = -1;
    int m_hoveredCopyBlock = -1;
    int m_copiedBlock = -1;
    bool m_selecting = false;
    bool m_selectable = true;
    bool m_taskListInteractive = false;
};

} // namespace ui::widget
