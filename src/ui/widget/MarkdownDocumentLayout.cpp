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

const ui::markdown::DocumentLayout* MarkdownDocumentLayout::findCachedLayout(qreal width) const
{
    if (m_controller->isStreaming() || m_stableLayoutDirty) return nullptr;
    for (auto it = m_layoutCacheList.begin(); it != m_layoutCacheList.end(); ++it) {
        if (qAbs(it->width - width) < 0.5 && it->themeVersion == m_theme.version && !it->layout.blocks.isEmpty()) {
            if (it != m_layoutCacheList.begin()) {
                std::rotate(m_layoutCacheList.begin(), it, it + 1);
            }
            return &m_layoutCacheList.front().layout;
        }
    }
    return nullptr;
}

void MarkdownDocumentLayout::insertCachedLayout(qreal width, ui::markdown::DocumentLayout layout) const
{
    if (m_controller->isStreaming() || layout.blocks.isEmpty()) return;
    for (auto it = m_layoutCacheList.begin(); it != m_layoutCacheList.end(); ++it) {
        if (qAbs(it->width - width) < 0.5) {
            it->width = width;
            it->themeVersion = m_theme.version;
            it->layout = std::move(layout);
            if (it != m_layoutCacheList.begin()) {
                std::rotate(m_layoutCacheList.begin(), it, it + 1);
            }
            return;
        }
    }
    constexpr size_t kMaxCacheEntries = 6;
    if (m_layoutCacheList.size() >= kMaxCacheEntries) {
        m_layoutCacheList.pop_back();
    }
    m_layoutCacheList.insert(m_layoutCacheList.begin(), CacheEntry{width, m_theme.version, std::move(layout)});
}

void MarkdownDocumentLayout::invalidateLayoutCache()
{
    m_layoutCacheList.clear();
}

void MarkdownDocumentLayout::setWidth(qreal width)
{
    if (qAbs(m_width - width) < 0.5) return;
    m_width = width;

    if (!m_controller->isStreaming()) {
        if (const auto* cached = findCachedLayout(m_width)) {
            m_currentLayout = *cached;
            emit layoutReady(m_currentLayout);
            return;
        }
    }
    relayout();
}

void MarkdownDocumentLayout::setTheme(const ui::markdown::MarkdownTheme& theme)
{
    m_theme = theme;
    m_stableLayoutDirty = true;
    invalidateLayoutCache();
    relayout();
}

void MarkdownDocumentLayout::setImages(const QHash<QString, QImage>* images)
{
    m_images = images;
    m_stableLayoutDirty = true;
    invalidateLayoutCache();
    relayout();
}

void MarkdownDocumentLayout::forceRelayout()
{
    m_stableLayoutDirty = true;
    invalidateLayoutCache();
    relayout();
}

bool MarkdownDocumentLayout::updateImageSize(const QString& source, const QSize& newSize)
{
    if (newSize.isEmpty() || m_currentLayout.blocks.isEmpty()) return false;

    bool modified = false;
    for (int i = 0; i < m_currentLayout.blocks.size(); ++i) {
        auto& block = m_currentLayout.blocks[i];
        if (block.kind != ui::markdown::BlockKind::Image || block.imageUrl != source) continue;

        const qreal indent = block.rect.left();
        const qreal right = m_width - m_theme.contentMargins.right();
        const qreal maxW = right - indent;
        const qreal maxH = 600;
        const QSizeF intrinsic = newSize;
        qreal w = qMin<qreal>(intrinsic.width(), maxW);
        qreal h = intrinsic.height() * (w / qMax<qreal>(1.0, intrinsic.width()));
        if (h > maxH) {
            h = maxH;
            w = intrinsic.width() * (h / qMax<qreal>(1.0, intrinsic.height()));
        }

        const qreal deltaH = h - block.rect.height();
        if (qAbs(deltaH) < 0.5) continue;

        block.rect.setWidth(w);
        block.rect.setHeight(h);
        block.imageIntrinsicSize = intrinsic;

        for (int j = i + 1; j < m_currentLayout.blocks.size(); ++j) {
            auto& downstream = m_currentLayout.blocks[j];
            downstream.rect.translate(0, deltaH);
            downstream.copyButtonRect.translate(0, deltaH);
            if (downstream.taskItem) {
                downstream.taskCheckRect.translate(0, deltaH);
            }
        }

        m_currentLayout.size.rheight() += deltaH;
        modified = true;

        if (m_controller->isStreaming() && i < m_stableLayout.blocks.size()) {
            if (m_stableLayout.blocks[i].imageUrl == source) {
                m_stableLayout.blocks[i].rect.setWidth(w);
                m_stableLayout.blocks[i].rect.setHeight(h);
                m_stableLayout.blocks[i].imageIntrinsicSize = intrinsic;
                for (int j = i + 1; j < m_stableLayout.blocks.size(); ++j) {
                    auto& downstream = m_stableLayout.blocks[j];
                    downstream.rect.translate(0, deltaH);
                    downstream.copyButtonRect.translate(0, deltaH);
                    if (downstream.taskItem) {
                        downstream.taskCheckRect.translate(0, deltaH);
                    }
                }
                m_stableLayout.size.rheight() += deltaH;
            }
        }
    }

    if (modified) {
        emit layoutReady(m_currentLayout);
        return true;
    }
    return false;
}

ui::markdown::DocumentLayout MarkdownDocumentLayout::measure(qreal maxWidth) const
{
    const qreal w = qMax<qreal>(1, maxWidth);
    if (!m_controller->isStreaming() && !m_stableLayoutDirty) {
        if (const auto* cached = findCachedLayout(w)) {
            return *cached;
        }
    }

    static const QHash<QString, QImage> emptyImages;
    const auto& images = m_images ? *m_images : emptyImages;
    ui::markdown::DocumentLayout result;
    if (m_controller->isStreaming()) {
        result = m_engine.layout(m_controller->stableDocument(), m_controller->tailDocument(), w, m_theme, images);
    } else {
        result = m_engine.layout(m_controller->stableDocument(), w, m_theme, images);
        insertCachedLayout(w, result);
    }
    return result;
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
    invalidateLayoutCache();
    relayout();
}

void MarkdownDocumentLayout::onStableDocumentAppended()
{
    m_stableLayoutDirty = true;
    invalidateLayoutCache();
}

void MarkdownDocumentLayout::onTailDocumentChanged()
{
    invalidateLayoutCache();
    relayout();
}

void MarkdownDocumentLayout::relayout()
{
    static const QHash<QString, QImage> emptyImages;
    const auto& images = m_images ? *m_images : emptyImages;

    if (!m_controller->isStreaming()) {
        if (const auto* cached = findCachedLayout(m_width)) {
            m_currentLayout = *cached;
            emit layoutReady(m_currentLayout);
            return;
        }
        m_currentLayout = m_engine.layout(m_controller->stableDocument(), m_width, m_theme, images);
        insertCachedLayout(m_width, m_currentLayout);
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
