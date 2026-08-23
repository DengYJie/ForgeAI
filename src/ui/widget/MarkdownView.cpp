#include "MarkdownView.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <FluentQt/Design.h>

namespace ui::widget {

MarkdownView::MarkdownView(QWidget *parent)
    : QWidget(parent)
    , m_textEdit(new fluent::textfields::TextEdit(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_textEdit->setReadOnly(true);
    m_textEdit->setMinVisibleLines(1);
    m_textEdit->setMaxVisibleLines(99999);
    m_textEdit->setScrollChainingEnabled(true);
    m_textEdit->setUnfocusedBorderWidth(0);
    m_textEdit->setFocusedBorderWidth(0);
    m_textEdit->setContentMargins(QMargins(0, 2, 0, 4));
    m_textEdit->setLineHeight(24);
    layout->addWidget(m_textEdit);

    connect(m_textEdit, &fluent::textfields::TextEdit::layoutMetricsChanged, this, &MarkdownView::updateAutoFitHeight);
    connect(m_textEdit, &fluent::textfields::TextEdit::textChanged, this, &MarkdownView::updateAutoFitHeight);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

MarkdownView::~MarkdownView() = default;

void MarkdownView::setMarkdown(const QString &markdown)
{
    if (m_markdown == markdown) return;
    m_markdown = markdown;
    m_textEdit->setPlainText(markdown);
    updateAutoFitHeight();
}

QString MarkdownView::markdown() const
{
    return m_markdown;
}

void MarkdownView::setHtml(const QString &html)
{
    m_markdown = html;
    m_textEdit->setPlainText(html);
    updateAutoFitHeight();
}

QString MarkdownView::html() const
{
    return m_markdown;
}

void MarkdownView::beginStream()
{
    m_streaming = true;
    m_markdown.clear();
    m_textEdit->clear();
    emit streamingChanged(true);
    updateAutoFitHeight();
}

void MarkdownView::appendMarkdown(const QString &chunk)
{
    if (chunk.isEmpty()) return;
    m_markdown += chunk;
    m_textEdit->setPlainText(m_markdown);
    updateAutoFitHeight();
}

void MarkdownView::appendHtml(const QString &htmlFragment)
{
    appendMarkdown(htmlFragment);
}

void MarkdownView::finishStream()
{
    m_streaming = false;
    emit streamingChanged(false);
    updateAutoFitHeight();
}

bool MarkdownView::isStreaming() const
{
    return m_streaming;
}

void MarkdownView::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
}

QUrl MarkdownView::baseUrl() const
{
    return m_baseUrl;
}

void MarkdownView::scrollToAnchor(const QString &name)
{
    Q_UNUSED(name)
}

void MarkdownView::setMarkdownStyleSheet(const MarkdownStyleSheet &styleSheet)
{
    m_styleSheet = styleSheet;
    m_usesThemeStyleSheet = false;
}

MarkdownStyleSheet MarkdownView::markdownStyleSheet() const
{
    return m_styleSheet;
}

void MarkdownView::resetMarkdownStyleSheetToTheme()
{
    m_usesThemeStyleSheet = true;
}

void MarkdownView::setTransparentBackground(bool transparent)
{
    m_transparentBackground = transparent;
}

bool MarkdownView::isTransparentBackground() const
{
    return m_transparentBackground;
}

void MarkdownView::setAutoFitHeight(bool enable)
{
    if (m_autoFitHeight == enable) return;
    m_autoFitHeight = enable;
    updateAutoFitHeight();
}

bool MarkdownView::isAutoFitHeight() const
{
    return m_autoFitHeight;
}

void MarkdownView::onThemeUpdated()
{
    if (m_textEdit) {
        m_textEdit->onThemeUpdated();
    }
    updateAutoFitHeight();
}

void MarkdownView::setAllowNetworkAccess(bool allow)
{
    m_allowNetworkAccess = allow;
}

bool MarkdownView::allowNetworkAccess() const
{
    return m_allowNetworkAccess;
}

void MarkdownView::clearResourceCache()
{
}

void MarkdownView::setAllowHtml(bool allow)
{
    m_allowHtml = allow;
}

bool MarkdownView::allowHtml() const
{
    return m_allowHtml;
}

QString MarkdownView::selectedText() const
{
    return m_textEdit ? m_textEdit->toPlainText() : QString();
}

QString MarkdownView::selectedHtml() const
{
    return selectedText();
}

void MarkdownView::copy()
{
    if (m_textEdit) {
        QGuiApplication::clipboard()->setText(m_textEdit->toPlainText());
    }
}

void MarkdownView::setZoomFactor(qreal factor)
{
    m_zoomFactor = factor;
}

qreal MarkdownView::zoomFactor() const
{
    return m_zoomFactor;
}

bool MarkdownView::findText(const QString &text,
                            QTextDocument::FindFlags flags,
                            bool incremental,
                            bool *wrapped)
{
    Q_UNUSED(text)
    Q_UNUSED(flags)
    Q_UNUSED(incremental)
    if (wrapped) *wrapped = false;
    return false;
}

void MarkdownView::updateAutoFitHeight()
{
    if (!m_textEdit) return;
    int h = m_textEdit->minimumHeight();
    if (h <= 0) {
        h = m_textEdit->height();
    }
    if (h <= 0) {
        h = 24;
    }
    // 增加底部呼吸空间缓冲，确保末行字符下延伸部分与下划线完整展示不被裁切
    h += 10;
    if (h != m_autoFitContentHeight) {
        m_autoFitContentHeight = h;
        updateGeometry();
        emit autoFitHeightChanged(h);
    }
}

QSize MarkdownView::sizeHint() const
{
    int h = m_autoFitContentHeight > 0 ? m_autoFitContentHeight : (m_textEdit ? m_textEdit->minimumHeight() + 10 : 34);
    if (h <= 0) h = 34;
    return QSize(256, h);
}

QSize MarkdownView::minimumSizeHint() const
{
    return QSize(50, sizeHint().height());
}

void MarkdownView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateAutoFitHeight();
}

void MarkdownView::wheelEvent(QWheelEvent *event)
{
    QWidget::wheelEvent(event);
}

void MarkdownView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateAutoFitHeight();
}

} // namespace ui::widget
