#pragma once

#include "ui/markdown/MarkdownLayout.h"
#include "ui/markdown/MarkdownTheme.h"

#include <QObject>
#include <QSizeF>

namespace ui::widget {

class MarkdownDocumentController;

struct MarkdownDocumentLayoutMetrics {
    quint64 stableLayoutCount = 0;
    quint64 tailLayoutCount = 0;
    qint64 lastStableLayoutMs = 0;
    qint64 lastTailLayoutMs = 0;
};

class MarkdownDocumentLayout : public QObject
{
    Q_OBJECT

public:
    explicit MarkdownDocumentLayout(MarkdownDocumentController* controller, QObject* parent = nullptr);

    void setWidth(qreal width);
    void setTheme(const ui::markdown::MarkdownTheme& theme);
    void setImages(const QHash<QString, QImage>* images);
    void forceRelayout();
    bool updateImageSize(const QString& source, const QSize& newSize);
    ui::markdown::DocumentLayout measure(qreal maxWidth) const;

    const ui::markdown::DocumentLayout& currentLayout() const;
    MarkdownDocumentLayoutMetrics metrics() const;

signals:
    void layoutReady(const ui::markdown::DocumentLayout& layout);

private:
    struct CacheEntry {
        qreal width = -1;
        quint64 themeVersion = 0;
        ui::markdown::DocumentLayout layout;
    };

    const ui::markdown::DocumentLayout* findCachedLayout(qreal width) const;
    void insertCachedLayout(qreal width, ui::markdown::DocumentLayout layout) const;
    void invalidateLayoutCache();

    void onDocumentRebuilt();
    void onStableDocumentAppended();
    void onTailDocumentChanged();
    void relayout();

    MarkdownDocumentController* m_controller;
    ui::markdown::MarkdownLayoutEngine m_engine;
    ui::markdown::MarkdownTheme m_theme;
    ui::markdown::DocumentLayout m_currentLayout;
    ui::markdown::DocumentLayout m_stableLayout;
    qreal m_width = 800;
    bool m_stableLayoutDirty = true;
    qreal m_stableLayoutWidth = -1;
    quint64 m_stableLayoutThemeVersion = 0;
    const QHash<QString, QImage>* m_images = nullptr;
    MarkdownDocumentLayoutMetrics m_metrics;
    mutable std::vector<CacheEntry> m_layoutCacheList;
};

} // namespace ui::widget
