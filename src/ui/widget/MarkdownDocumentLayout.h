#pragma once

#include "ui/markdown/MarkdownLayout.h"
#include "ui/markdown/MarkdownTheme.h"

#include <QObject>
#include <QTimer>

namespace ui::widget {

class MarkdownDocumentController;

struct MarkdownDocumentLayoutMetrics {
    quint64 layoutCount = 0;
    quint64 blockCacheHits = 0;
    quint64 blockCacheMisses = 0;
    quint64 blockCacheEvictions = 0;
    qsizetype blockCacheEntries = 0;
    qsizetype blockCacheEstimatedBytes = 0;
    qsizetype blockCacheLimitBytes = 0;
    qint64 lastLayoutUs = 0;
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
    ui::markdown::DocumentLayoutPtr measure(qreal maxWidth) const;

    ui::markdown::DocumentLayoutPtr currentLayout() const { return m_currentLayout; }
    MarkdownDocumentLayoutMetrics metrics() const;

signals:
    void layoutReady(ui::markdown::DocumentLayoutPtr layout);

private:
    void onDocumentChanged();
    void relayout();

    MarkdownDocumentController* m_controller;
    mutable ui::markdown::MarkdownLayoutEngine m_engine;
    ui::markdown::MarkdownTheme m_theme;
    ui::markdown::DocumentLayoutPtr m_currentLayout;
    qreal m_width = 800;
    qreal m_pendingWidth = 800;
    QTimer m_resizeTimer;
    const QHash<QString, QImage>* m_images = nullptr;
};

} // namespace ui::widget
