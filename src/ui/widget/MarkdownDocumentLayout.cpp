#include "MarkdownDocumentLayout.h"
#include "MarkdownDocumentController.h"

namespace ui::widget {

MarkdownDocumentLayout::MarkdownDocumentLayout(MarkdownDocumentController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
{
    connect(controller, &MarkdownDocumentController::documentChanged,
            this, &MarkdownDocumentLayout::onDocumentChanged);
    m_resizeTimer.setSingleShot(true);
    m_resizeTimer.setInterval(16);
    connect(&m_resizeTimer, &QTimer::timeout, this, [this] {
        if (qAbs(m_width - m_pendingWidth) < 1.0 / 64.0) return;
        m_width = m_pendingWidth;
        relayout();
    });
}

void MarkdownDocumentLayout::setWidth(qreal width)
{
    width = qMax<qreal>(1, width);
    m_pendingWidth = width;
    if (!m_currentLayout || m_currentLayout->blocks.isEmpty()) {
        m_width = width;
        relayout();
        return;
    }
    if (qAbs(m_width - width) < 1.0 / 64.0) return;
    m_resizeTimer.start();
}

void MarkdownDocumentLayout::setTheme(const ui::markdown::MarkdownTheme& theme)
{
    m_theme = theme;
    relayout();
}

void MarkdownDocumentLayout::setImages(const QHash<QString, QImage>* images)
{
    m_images = images;
    relayout();
}

void MarkdownDocumentLayout::forceRelayout()
{
    relayout();
}

bool MarkdownDocumentLayout::updateImageSize(const QString& source, const QSize& newSize)
{
    Q_UNUSED(newSize);
    if (!m_currentLayout) return false;
    bool found = false;
    for (const auto& block : m_currentLayout->blocks) {
        if (block.kind == ui::markdown::BlockKind::Image && block.imageUrl == source) {
            found = true;
            break;
        }
    }
    if (found) relayout();
    return found;
}

ui::markdown::DocumentLayoutPtr MarkdownDocumentLayout::measure(qreal maxWidth) const
{
    static const QHash<QString, QImage> emptyImages;
    const auto& images = m_images ? *m_images : emptyImages;
    return std::make_shared<ui::markdown::DocumentLayout>(
        m_engine.layout(m_controller->document(), qMax<qreal>(1, maxWidth), m_theme, images));
}

MarkdownDocumentLayoutMetrics MarkdownDocumentLayout::metrics() const
{
    const auto metrics = m_engine.metrics();
    return {metrics.totalLayouts, metrics.blockCacheHits, metrics.blockCacheMisses,
            metrics.blockCacheEvictions, metrics.blockCacheEntries,
            metrics.blockCacheEstimatedBytes, metrics.blockCacheLimitBytes,
            metrics.lastLayoutUs};
}

void MarkdownDocumentLayout::onDocumentChanged()
{
    QVector<ui::markdown::BlockId> removed;
    const auto changes = m_controller->metrics().lastChanges;
    for (const auto& change : changes.changes) {
        if (change.kind == ui::markdown::BlockChangeKind::Removed)
            removed.push_back(change.id);
    }
    m_engine.removeBlocks(removed);
    relayout();
}

void MarkdownDocumentLayout::relayout()
{
    static const QHash<QString, QImage> emptyImages;
    const auto& images = m_images ? *m_images : emptyImages;
    m_currentLayout = std::make_shared<ui::markdown::DocumentLayout>(
        m_engine.layout(m_controller->document(), m_width, m_theme, images));
    emit layoutReady(m_currentLayout);
}

} // namespace ui::widget
