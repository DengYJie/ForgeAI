#pragma once

#include <QPoint>
#include <QString>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QAbstractScrollArea>
#include <memory>
#include <optional>

#include <FluentQt/Foundation.h>
#include "MarkdownStyle.h"
#include "MarkdownDocumentController.h"
#include "MarkdownDocumentLayout.h"
#include "MarkdownViewEventFilter.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/markdown/MarkdownTheme.h"
#include "ui/markdown/resource/MarkdownImageResourceManager.h"

class QVariantAnimation;

namespace ui::widget {

struct MarkdownViewMetrics {
    quint64 parseCount = 0;
    quint64 scheduledUpdateCount = 0;
    quint64 coalescedChunkCount = 0;
    quint64 unchangedBlockCount = 0;
    quint64 updatedBlockCount = 0;
    quint64 layoutCount = 0;
    quint64 blockCacheHits = 0;
    quint64 blockCacheMisses = 0;
    quint64 blockCacheEvictions = 0;
    qsizetype blockCacheEntries = 0;
    qsizetype blockCacheEstimatedBytes = 0;
    qsizetype blockCacheLimitBytes = 0;
    qint64 lastParseUs = 0;
    qint64 lastLayoutUs = 0;
    qint64 lastPaintMs = 0;
    int blockCount = 0;
    int visibleBlockCount = 0;
    qreal documentHeight = 0;
};

class MarkdownView : public QAbstractScrollArea, public fluent::FluentElement
{
    Q_OBJECT

public:
    explicit MarkdownView(QWidget *parent = nullptr);
    ~MarkdownView() override;

    void setMarkdown(const QString &markdown);
    QString markdown() const;
    void clear();

    void beginStream();
    void appendMarkdown(const QString &chunk);
    void appendStreamingText(const QString &chunk);
    void finishStream();
    void finishStreaming();
    bool isStreaming() const;
    MarkdownViewMetrics metrics() const noexcept;

    void setBaseUrl(const QUrl &url);
    QUrl baseUrl() const;
    void scrollToAnchor(const QString &name);

    void setMarkdownStyleSheet(const MarkdownStyleSheet &styleSheet);
    MarkdownStyleSheet markdownStyleSheet() const;
    void resetMarkdownStyleSheetToTheme();
    void setTheme(const ui::markdown::MarkdownTheme& theme);
    ui::markdown::MarkdownTheme theme() const;
    void setBaseFont(const QFont& font);
    void setContentMargins(const QMarginsF& margins);
    void setTransparentBackground(bool transparent);
    bool isTransparentBackground() const;
    enum class HorizontalSizingMode {
        FillAvailable,
        FitContent
    };

    static constexpr qreal DefaultPreferredWidthLimit = 800.0;

    void setHorizontalSizingMode(HorizontalSizingMode mode);
    HorizontalSizingMode horizontalSizingMode() const;

    void setPreferredWidthLimit(qreal width);
    qreal preferredWidthLimit() const;

    void setAutoFitHeight(bool enable);
    bool isAutoFitHeight() const;

    ui::markdown::BlockScrollOffset blockScrollOffset(ui::markdown::BlockId blockId, ui::markdown::ElementId elementId) const;
    void setBlockScrollOffset(ui::markdown::BlockId blockId, ui::markdown::ElementId elementId,
                              const ui::markdown::BlockScrollOffset& offset);
    bool scrollBlock(ui::markdown::BlockId blockId, ui::markdown::ElementId elementId,
                     qreal deltaX, qreal deltaY, bool smooth = true);
    void onThemeUpdated() override;

    void setAllowNetworkAccess(bool allow);
    bool allowNetworkAccess() const;
    void clearResourceCache();
    void setImageLoadingEnabled(bool enabled);
    bool imageLoadingEnabled() const;

    void setAllowHtml(bool allow);
    bool allowHtml() const;

    QString selectedText() const;
    QString selectedHtml() const;
    void copy();
    void selectAll();
    void setSelectable(bool selectable);
    bool isSelectable() const;
    void setTaskListInteractive(bool interactive);
    bool isTaskListInteractive() const;

    void setZoomFactor(qreal factor);
    qreal zoomFactor() const;

    bool findText(const QString &text,
                  QTextDocument::FindFlags flags,
                  bool incremental,
                  bool *wrapped = nullptr);

    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void linkActivated(const QUrl &url);
    void linkHighlighted(const QUrl &url);
    void formControlActivated(const QString &tag, const QString &type, const QString &name, const QString &value, bool checked);
    void detailsToggled(const QString &id, bool open);
    void copyAvailable(bool available);
    void selectionChanged(bool hasSelection);
    void documentSizeChanged(const QSizeF &size);
    void imageActivated(const QUrl &url);
    void taskToggled(int sourceLine, bool checked);
    void contextMenuRequested(const QPoint &pos, const QUrl &linkUrl, const QUrl &imageUrl);
    void streamingChanged(bool streaming);
    void streamingFinished();
    void autoFitHeightChanged(int height);

protected:
    bool viewportEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void onLayoutReady(ui::markdown::DocumentLayoutPtr layout);
    void onTaskToggleRequested(int blockIndex);
    void onRepaintRequested();
    void updateScrollBars();
    void updateAutoFitHeight();
    void paintViewport(QPaintEvent *event);
    QPointF toDocument(const QPointF &viewportPosition) const;
    QString documentPlainText() const;
    void requestImageResources();
    bool handleBlockWheel(QWheelEvent* event);
    int elementIndex(ui::markdown::BlockId blockId, ui::markdown::ElementId elementId) const;
    bool scrollBlockAtIndex(int blockIndex, qreal deltaX, qreal deltaY, bool smooth);
    void invalidatePreferredSize();
    void recalculatePreferredSize();
    void updateActualLayoutWidth();

    MarkdownDocumentController* m_controller;
    MarkdownDocumentLayout* m_layoutCache;
    MarkdownViewEventFilter* m_eventFilter;
    ui::markdown::MarkdownRenderer m_renderer;
    ui::markdown::MarkdownTheme m_theme;
    ui::markdown::MarkdownImageResourceManager m_resources;
    ui::markdown::DocumentLayoutPtr m_documentLayout = std::make_shared<ui::markdown::DocumentLayout>();

    QUrl m_baseUrl;
    MarkdownStyleSheet m_styleSheet;
    std::optional<QMarginsF> m_customContentMargins;
    HorizontalSizingMode m_horizontalSizingMode = HorizontalSizingMode::FillAvailable;
    qreal m_preferredWidthLimit = 0;
    QSizeF m_preferredContentSize;
    bool m_preferredSizeDirty = true;
    bool m_usesThemeStyleSheet = true;
    bool m_transparentBackground = true;
    bool m_autoFitHeight = false;
    int m_lastAutoFitHeight = -1;
    qreal m_zoomFactor = 1.0;
    bool m_allowNetworkAccess = true;
    bool m_wasStreaming = false; // Tracks streaming→not-streaming transition for streamingFinished emit

    QHash<int, ui::markdown::BlockScrollOffset> m_blockScrollOffsets;
    QHash<int, ui::markdown::BlockScrollOffset> m_blockTargetScrollOffsets;
    QHash<int, QVariantAnimation*> m_blockScrollAnimations;

    QSizeF m_lastDocumentSize;
    MarkdownViewMetrics m_metrics;
};

} // namespace ui::widget
