#include "MarkdownDocumentLayout.h"
#include "MarkdownDocumentController.h"

#include <QElapsedTimer>

namespace ui::widget {

MarkdownDocumentLayout::MarkdownDocumentLayout(MarkdownDocumentController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
{
    connect(controller, &MarkdownDocumentController::documentRebuilt,
            this, &MarkdownDocumentLayout::onDocumentRebuilt);
    connect(controller, &MarkdownDocumentController::stableDocumentAppended,
            this, &MarkdownDocumentLayout::onStableDocumentAppended);
    connect(controller, &MarkdownDocumentController::tailDocumentChanged,
            this, &MarkdownDocumentLayout::onTailDocumentChanged);
}

void MarkdownDocumentLayout::setWidth(qreal width)
{
    if (qAbs(m_width - width) < 0.5) return;
    m_width = width;
    relayout();
}

void MarkdownDocumentLayout::setTheme(const ui::markdown::MarkdownTheme& theme)
{
    m_theme = theme;
    m_stableLayoutDirty = true;
    relayout();
}

void MarkdownDocumentLayout::setImages(const QHash<QString, QImage>* images)
{
    m_images = images;
    m_stableLayoutDirty = true;
    relayout();
}

void MarkdownDocumentLayout::forceRelayout()
{
    m_stableLayoutDirty = true;
    relayout();
}

const ui::markdown::DocumentLayout& MarkdownDocumentLayout::currentLayout() const
{
    return m_currentLayout;
}

MarkdownDocumentLayoutMetrics MarkdownDocumentLayout::metrics() const
{
    return m_metrics;
}

void MarkdownDocumentLayout::onDocumentRebuilt()
{
    m_stableLayoutDirty = true;
    relayout();
}

void MarkdownDocumentLayout::onStableDocumentAppended()
{
    m_stableLayoutDirty = true;
}

void MarkdownDocumentLayout::onTailDocumentChanged()
{
    relayout();
}

void MarkdownDocumentLayout::relayout()
{
    static const QHash<QString, QImage> emptyImages;
    const auto& images = m_images ? *m_images : emptyImages;

    if (!m_controller->isStreaming()) {
        m_currentLayout = m_engine.layout(m_controller->stableDocument(), m_width, m_theme, images);
        emit layoutReady(m_currentLayout);
        return;
    }

    if (m_stableLayoutDirty || !qFuzzyCompare(m_stableLayoutWidth, m_width) || m_stableLayoutThemeVersion != m_theme.version) {
        QElapsedTimer t; t.start();
        m_stableLayout = m_engine.layout(m_controller->stableDocument(), m_width, m_theme, images);
        m_metrics.lastStableLayoutMs = t.elapsed();
        ++m_metrics.stableLayoutCount;
        m_stableLayoutDirty = false;
        m_stableLayoutWidth = m_width;
        m_stableLayoutThemeVersion = m_theme.version;
    }

    QElapsedTimer t; t.start();
    const ui::markdown::DocumentLayout tail = m_engine.layout(m_controller->tailDocument(), m_width, m_theme, images);
    m_metrics.lastTailLayoutMs = t.elapsed();
    ++m_metrics.tailLayoutCount;

    m_currentLayout = m_stableLayout;
    const qreal yOffset = m_stableLayout.size.height() - m_theme.contentMargins.bottom() - m_theme.contentMargins.top();
    const int textOffset = m_stableLayout.textLength();
    for (ui::markdown::BlockLayout block : tail.blocks) {
        block.rect.translate(0, yOffset);
        block.copyButtonRect.translate(0, yOffset);
        block.documentTextOffset += textOffset;
        m_currentLayout.blocks.push_back(std::move(block));
    }
    m_currentLayout.size = QSizeF(m_width, yOffset + tail.size.height());
    m_currentLayout.width = m_width;
    m_currentLayout.themeVersion = m_theme.version;

    emit layoutReady(m_currentLayout);
}

} // namespace ui::widget
