#pragma once

#include <QPoint>
#include <QString>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <memory>

#include <FluentQt/Foundation.h>
#include "qlitehtml_types.h"
#include "MarkdownStyle.h"

class QLiteHtmlWidget;

namespace core::markdown {
class MarkdownRenderer;
}

namespace ui::widget {

/**
 * @brief Fluent Design 2-styled Markdown rendering and streaming viewer widget.
 *
 * Backed by md4c (GFM parsing) and QLiteHtmlWidget (high-performance HTML rendering engine).
 */
class MarkdownView : public QWidget, public fluent::FluentElement
{
    Q_OBJECT

public:
    using ResourceHandler = qlitehtml::ResourceHandler;
    using ResourceType = qlitehtml::ResourceType;

    explicit MarkdownView(QWidget *parent = nullptr);
    ~MarkdownView() override;

    // Document loading
    void setMarkdown(const QString &markdown);
    QString markdown() const;

    // Incremental streaming
    void beginStream();
    void appendMarkdown(const QString &chunk);
    void finishStream();
    bool isStreaming() const;

    // Base URL & Anchor navigation
    void setBaseUrl(const QUrl &url);
    QUrl baseUrl() const;
    void scrollToAnchor(const QString &name);

    // Styling & Theme
    void setMarkdownStyleSheet(const MarkdownStyleSheet &styleSheet);
    MarkdownStyleSheet markdownStyleSheet() const;
    void resetMarkdownStyleSheetToTheme();
    void onThemeUpdated() override;

    // Resource & Security Pipeline
    void setAllowNetworkAccess(bool allow);
    bool allowNetworkAccess() const;
    void setResourceHandler(const ResourceHandler &handler);
    void clearResourceCache();

    // HTML / Extension Configuration
    void setAllowHtml(bool allow);
    bool allowHtml() const;

    // Text Selection, Clipboard & Search
    QString selectedText() const;
    QString selectedHtml() const;
    void copy();

    void setZoomFactor(qreal factor);
    qreal zoomFactor() const;

    bool findText(const QString &text,
                  QTextDocument::FindFlags flags,
                  bool incremental,
                  bool *wrapped = nullptr);

signals:
    void linkActivated(const QUrl &url);
    void linkHighlighted(const QUrl &url);
    void formControlActivated(const QString &tag, const QString &type, const QString &name, const QString &value, bool checked);
    void detailsToggled(const QString &id, bool open);
    void copyAvailable(bool available);
    void contextMenuRequested(const QPoint &pos, const QUrl &linkUrl, const QUrl &imageUrl);
    void streamingChanged(bool streaming);

private:
    void handleLinkClicked(const QUrl &url);
    void processCompleteLines();
    void commitCurrentBlock();
    void previewCurrentBlock();
    void scheduleCurrentBlockPreview();
    void resetBlockState();
    void rebuildDocument();
    MarkdownStyleSheet themeStyleSheet() const;
    static bool fenceOpener(const QString &line, QChar *marker, int *length);
    static bool isPreviewableParagraphStart(const QString &line);
    static bool containsGfmTable(const QString &markdown);
    static bool isAtxHeading(const QString &line);
    bool fenceCloser(const QString &line) const;

    std::unique_ptr<core::markdown::MarkdownRenderer> m_renderer;
    QLiteHtmlWidget *m_htmlView = nullptr;
    QString m_markdown;
    QString m_renderedHtml;
    QString m_lineBuffer;
    QString m_currentBlock;
    QString m_pendingPreviewHtml;
    QString m_fenceContent;
    QUrl m_baseUrl;
    MarkdownStyleSheet m_styleSheet;
    bool m_usesThemeStyleSheet = true;
    bool m_streaming = false;
    bool m_blockHasContent = false;
    bool m_currentBlockPreviewable = false;
    bool m_inFence = false;
    QChar m_fenceMarker;
    int m_fenceLength = 0;
    QTimer m_previewTimer;
};

} // namespace ui::widget
