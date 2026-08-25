#pragma once

#include <QPoint>
#include <QString>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QAbstractScrollArea>
#include <memory>

#include <FluentQt/Foundation.h>
#include <FluentQt/TextFields.h>
#include "MarkdownStyle.h"
#include "ui/markdown/MarkdownDocument.h"
#include "ui/markdown/MarkdownLayout.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/markdown/MarkdownTheme.h"
#include "ui/markdown/resource/MarkdownImageResourceManager.h"

namespace ui::widget {

struct MarkdownViewMetrics {
    quint64 fullParseCount = 0;
    quint64 stableParseCount = 0;
    quint64 tailParseCount = 0;
    quint64 stableLayoutCount = 0;
    quint64 tailLayoutCount = 0;
    qint64 lastParseMs = 0;
    qint64 lastStableLayoutMs = 0;
    qint64 lastTailLayoutMs = 0;
    qint64 lastPaintMs = 0;
    int blockCount = 0;
    int visibleBlockCount = 0;
    qreal documentHeight = 0;
};

/**
 * @brief Self-painted CommonMark/GFM viewer.
 * Parsing, layout and painting are deliberately separated; this class only owns
 * input, scrolling, viewport dispatch and interaction state.
 */
class MarkdownView : public QAbstractScrollArea, public fluent::FluentElement
{
    Q_OBJECT

public:
    explicit MarkdownView(QWidget *parent = nullptr);
    ~MarkdownView() override;

    // Document loading
    void setMarkdown(const QString &markdown);
    QString markdown() const;
    void clear();
    void setHtml(const QString &html);
    QString html() const;

    // Incremental streaming
    void beginStream();
    void appendMarkdown(const QString &chunk);
    void appendStreamingText(const QString &chunk);
    void appendHtml(const QString &htmlFragment);
    void finishStream();
    void finishStreaming();
    bool isStreaming() const;
    MarkdownViewMetrics metrics() const noexcept;

    // Base URL & Anchor navigation
    void setBaseUrl(const QUrl &url);
    QUrl baseUrl() const;
    void scrollToAnchor(const QString &name);

    // Styling & Theme
    void setMarkdownStyleSheet(const MarkdownStyleSheet &styleSheet);
    MarkdownStyleSheet markdownStyleSheet() const;
    void resetMarkdownStyleSheetToTheme();
    void setTheme(const ui::markdown::MarkdownTheme& theme);
    ui::markdown::MarkdownTheme theme() const;
    void setBaseFont(const QFont& font);
    void setContentMargins(const QMarginsF& margins);
    void setTransparentBackground(bool transparent);
    bool isTransparentBackground() const;
    void setAutoFitHeight(bool enable);
    bool isAutoFitHeight() const;
    void setMaxContentWidth(qreal maxWidth);
    qreal maxContentWidth() const;
    void onThemeUpdated() override;

    // Resource & Security Pipeline
    void setAllowNetworkAccess(bool allow);
    bool allowNetworkAccess() const;
    void clearResourceCache();
    void setImageLoadingEnabled(bool enabled);
    bool imageLoadingEnabled() const;

    // HTML / Extension Configuration
    void setAllowHtml(bool allow);
    bool allowHtml() const;

    // Text Selection, Clipboard & Search
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
    void showEvent(QShowEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void rebuildDocument();
    void relayout();
    void updateScrollBars();
    void requestImageResources();
    void updateAutoFitHeight();
    void paintViewport(QPaintEvent *event);
    QPointF toDocument(const QPointF &viewportPosition) const;
    QString documentPlainText() const;
    void setSelectionPosition(int position, bool extend);
    void showCopiedFeedback(int blockIndex);
    void toggleTask(const ui::markdown::BlockLayout& block);
    qsizetype stableStreamingBoundary() const;

    QString m_markdown;
    QString m_streamTail;
    QUrl m_baseUrl;
    MarkdownStyleSheet m_styleSheet;
    bool m_usesThemeStyleSheet = true;
    bool m_transparentBackground = true;
    bool m_autoFitHeight = false;
    int m_autoFitContentHeight = 1;
    qreal m_maxContentWidth = 0;
    bool m_streaming = false;
    bool m_allowNetworkAccess = true;
    bool m_allowHtml = true;
    qreal m_zoomFactor = 1.0;
    ui::markdown::MarkdownParser m_parser;
    ui::markdown::MarkdownDocument m_document;
    ui::markdown::MarkdownDocument m_activeTailDocument;
    ui::markdown::MarkdownLayoutEngine m_layoutEngine;
    ui::markdown::DocumentLayout m_documentLayout;
    ui::markdown::DocumentLayout m_stableStreamLayout;
    ui::markdown::MarkdownRenderer m_renderer;
    ui::markdown::MarkdownTheme m_theme;
    ui::markdown::MarkdownImageResourceManager m_resources;
    ui::markdown::TextSelection m_selection;
    int m_selectionAnchor = -1;
    int m_hoveredBlock = -1;
    int m_copiedBlock = -1;
    bool m_selecting = false;
    bool m_selectable = true;
    bool m_taskListInteractive = false;
    bool m_layoutDirty = true;
    bool m_stableStreamLayoutDirty = true;
    qreal m_stableStreamLayoutWidth = -1;
    quint64 m_stableStreamThemeVersion = 0;
    QSizeF m_lastDocumentSize;
    MarkdownViewMetrics m_metrics;
};

} // namespace ui::widget
