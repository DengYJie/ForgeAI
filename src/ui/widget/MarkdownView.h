#pragma once

#include <QPoint>
#include <QString>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <memory>

#include <FluentQt/Foundation.h>
#include <FluentQt/TextFields.h>
#include "MarkdownStyle.h"

namespace ui::widget {

/**
 * @brief Fluent Design 2-styled Markdown / PlainText viewer widget.
 * Backed by fluent::textfields::TextEdit.
 */
class MarkdownView : public QWidget, public fluent::FluentElement
{
    Q_OBJECT

public:
    explicit MarkdownView(QWidget *parent = nullptr);
    ~MarkdownView() override;

    // Document loading
    void setMarkdown(const QString &markdown);
    QString markdown() const;
    void setHtml(const QString &html);
    QString html() const;

    // Incremental streaming
    void beginStream();
    void appendMarkdown(const QString &chunk);
    void appendHtml(const QString &htmlFragment);
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
    void setTransparentBackground(bool transparent);
    bool isTransparentBackground() const;
    void setAutoFitHeight(bool enable);
    bool isAutoFitHeight() const;
    void onThemeUpdated() override;

    // Resource & Security Pipeline
    void setAllowNetworkAccess(bool allow);
    bool allowNetworkAccess() const;
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

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void linkActivated(const QUrl &url);
    void linkHighlighted(const QUrl &url);
    void formControlActivated(const QString &tag, const QString &type, const QString &name, const QString &value, bool checked);
    void detailsToggled(const QString &id, bool open);
    void copyAvailable(bool available);
    void contextMenuRequested(const QPoint &pos, const QUrl &linkUrl, const QUrl &imageUrl);
    void streamingChanged(bool streaming);
    void autoFitHeightChanged(int height);

protected:
    void showEvent(QShowEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateAutoFitHeight();

    fluent::textfields::TextEdit *m_textEdit = nullptr;
    QString m_markdown;
    QUrl m_baseUrl;
    MarkdownStyleSheet m_styleSheet;
    bool m_usesThemeStyleSheet = true;
    bool m_transparentBackground = true;
    bool m_autoFitHeight = false;
    int m_autoFitContentHeight = 1;
    bool m_streaming = false;
    bool m_allowNetworkAccess = true;
    bool m_allowHtml = true;
    qreal m_zoomFactor = 1.0;
};

} // namespace ui::widget
