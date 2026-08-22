#include "MarkdownView.h"

#include "core/markdown/MarkdownRenderer.h"
#include "qlitehtmlwidget.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QScrollBar>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <FluentQt/Design.h>
#include <FluentQt/Scrolling.h>

namespace ui::widget {

namespace {
constexpr auto kCommittedStreamElementId = "markdown-stream-content";
constexpr auto kPendingStreamElementId = "markdown-stream-pending";
constexpr int kStreamFollowEndTolerance = 5;
constexpr int kParagraphPreviewDelayMs = 80;

class HtmlWidget : public QLiteHtmlWidget, public fluent::FluentElement {
public:
    explicit HtmlWidget(QWidget *parent = nullptr) : QLiteHtmlWidget(parent) {
        setVerticalScrollBar(new fluent::scrolling::ScrollBar(Qt::Vertical, this));
        setHorizontalScrollBar(new fluent::scrolling::ScrollBar(Qt::Horizontal, this));

        // Transparent borderless viewport matching Fluent layer backgrounds
        setFrameShape(QFrame::NoFrame);

        QPalette pal = palette();
        pal.setColor(QPalette::Window, Qt::transparent);
        pal.setColor(QPalette::Base, Qt::transparent);
        setPalette(pal);
        viewport()->setPalette(pal);

        setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setAttribute(Qt::WA_TranslucentBackground);
    }
};
} // namespace

MarkdownView::MarkdownView(QWidget *parent)
    : QWidget(parent)
    , m_renderer(std::make_unique<core::markdown::MarkdownRenderer>())
    , m_htmlView(new HtmlWidget(this))
{
    m_styleSheet = themeStyleSheet();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_htmlView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_htmlView);

    m_htmlView->setDefaultFont(themeFont(Typography::FontRole::Body).toQFont());

    // Connect and forward QLiteHtmlWidget signals
    connect(m_htmlView, &QLiteHtmlWidget::linkClicked, this, &MarkdownView::handleLinkClicked);
    connect(m_htmlView, &QLiteHtmlWidget::linkHighlighted, this, &MarkdownView::linkHighlighted);
    connect(m_htmlView, &QLiteHtmlWidget::formControlActivated, this, &MarkdownView::formControlActivated);
    connect(m_htmlView, &QLiteHtmlWidget::detailsToggled, this, &MarkdownView::detailsToggled);
    connect(m_htmlView, &QLiteHtmlWidget::copyAvailable, this, &MarkdownView::copyAvailable);
    connect(m_htmlView, &QLiteHtmlWidget::contextMenuRequested, this, &MarkdownView::contextMenuRequested);

    m_previewTimer.setInterval(kParagraphPreviewDelayMs);
    m_previewTimer.setSingleShot(true);
    connect(&m_previewTimer, &QTimer::timeout, this, &MarkdownView::previewCurrentBlock);

    rebuildDocument();
}

MarkdownView::~MarkdownView() = default;

void MarkdownView::setMarkdown(const QString &markdown)
{
    m_previewTimer.stop();
    m_markdown = markdown;
    m_renderedHtml = m_renderer->renderFragment(m_markdown);
    m_pendingPreviewHtml.clear();
    m_lineBuffer.clear();
    m_currentBlock.clear();
    resetBlockState();
    if (m_streaming) {
        m_streaming = false;
        emit streamingChanged(false);
    }
    rebuildDocument();
}

QString MarkdownView::markdown() const
{
    return m_markdown;
}

void MarkdownView::beginStream()
{
    const bool wasStreaming = m_streaming;
    m_previewTimer.stop();
    m_markdown.clear();
    m_renderedHtml.clear();
    m_pendingPreviewHtml.clear();
    m_lineBuffer.clear();
    m_currentBlock.clear();
    resetBlockState();
    m_streaming = true;
    rebuildDocument();
    if (!wasStreaming)
        emit streamingChanged(true);
}

void MarkdownView::appendMarkdown(const QString &chunk)
{
    if (chunk.isEmpty())
        return;
    if (!m_streaming)
        beginStream();

    m_markdown += chunk;
    m_lineBuffer += chunk;
    processCompleteLines();
    scheduleCurrentBlockPreview();
}

void MarkdownView::finishStream()
{
    if (!m_streaming)
        return;

    const int horizontal = m_htmlView->horizontalScrollBar()->value();
    const int vertical = m_htmlView->verticalScrollBar()->value();
    const int verticalMax = m_htmlView->verticalScrollBar()->maximum();
    const bool isAtBottom = verticalMax == 0 || vertical >= verticalMax - kStreamFollowEndTolerance;

    // The final full parse fixes document-wide Markdown constructs such as
    // reference links and list continuation without re-rendering per token.
    m_renderedHtml = m_renderer->renderFragment(m_markdown);
    m_previewTimer.stop();
    m_pendingPreviewHtml.clear();
    m_lineBuffer.clear();
    m_currentBlock.clear();
    resetBlockState();
    m_streaming = false;
    rebuildDocument();

    m_htmlView->horizontalScrollBar()->setValue(
        qBound(m_htmlView->horizontalScrollBar()->minimum(), horizontal, m_htmlView->horizontalScrollBar()->maximum()));

    if (isAtBottom) {
        m_htmlView->verticalScrollBar()->setValue(m_htmlView->verticalScrollBar()->maximum());
    } else {
        m_htmlView->verticalScrollBar()->setValue(
            qBound(m_htmlView->verticalScrollBar()->minimum(), vertical, m_htmlView->verticalScrollBar()->maximum()));
    }

    emit streamingChanged(false);
}

bool MarkdownView::isStreaming() const
{
    return m_streaming;
}

void MarkdownView::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
    m_htmlView->setUrl(url);
}

QUrl MarkdownView::baseUrl() const
{
    return m_baseUrl;
}

void MarkdownView::scrollToAnchor(const QString &name)
{
    m_htmlView->scrollToAnchor(name);
}

void MarkdownView::setMarkdownStyleSheet(const MarkdownStyleSheet &styleSheet)
{
    m_styleSheet = styleSheet;
    m_usesThemeStyleSheet = false;
    onThemeUpdated();
}

MarkdownStyleSheet MarkdownView::markdownStyleSheet() const
{
    return m_styleSheet;
}

void MarkdownView::resetMarkdownStyleSheetToTheme()
{
    m_usesThemeStyleSheet = true;
    m_styleSheet = themeStyleSheet();
    onThemeUpdated();
}

void MarkdownView::onThemeUpdated()
{
    const int horizontal = m_htmlView->horizontalScrollBar()->value();
    const int vertical = m_htmlView->verticalScrollBar()->value();
    if (m_usesThemeStyleSheet)
        m_styleSheet = themeStyleSheet();
    m_htmlView->setDefaultFont(themeFont(Typography::FontRole::Body).toQFont());
    rebuildDocument();
    m_htmlView->horizontalScrollBar()->setValue(
        qBound(m_htmlView->horizontalScrollBar()->minimum(), horizontal, m_htmlView->horizontalScrollBar()->maximum()));
    m_htmlView->verticalScrollBar()->setValue(
        qBound(m_htmlView->verticalScrollBar()->minimum(), vertical, m_htmlView->verticalScrollBar()->maximum()));
    update();
}

void MarkdownView::setAllowNetworkAccess(bool allow)
{
    m_htmlView->setAllowNetworkAccess(allow);
}

bool MarkdownView::allowNetworkAccess() const
{
    return m_htmlView->allowNetworkAccess();
}

void MarkdownView::setResourceHandler(const ResourceHandler &handler)
{
    m_htmlView->setResourceHandler(handler);
}

void MarkdownView::clearResourceCache()
{
    m_htmlView->clearResourceCache();
}

void MarkdownView::setAllowHtml(bool allow)
{
    m_renderer->setAllowHtml(allow);
    if (!m_markdown.isEmpty()) {
        m_renderedHtml = m_renderer->renderFragment(m_markdown);
        rebuildDocument();
    }
}

bool MarkdownView::allowHtml() const
{
    return m_renderer->allowHtml();
}

QString MarkdownView::selectedText() const
{
    return m_htmlView->selectedText();
}

QString MarkdownView::selectedHtml() const
{
    return m_htmlView->selectedHtml();
}

void MarkdownView::copy()
{
    const QString text = selectedText();
    if (!text.isEmpty()) {
        if (auto *cb = QGuiApplication::clipboard()) {
            cb->setText(text);
        }
    }
}

void MarkdownView::setZoomFactor(qreal factor)
{
    m_htmlView->setZoomFactor(factor);
}

qreal MarkdownView::zoomFactor() const
{
    return m_htmlView->zoomFactor();
}

bool MarkdownView::findText(const QString &text,
                            QTextDocument::FindFlags flags,
                            bool incremental,
                            bool *wrapped)
{
    return m_htmlView->findText(text, flags, incremental, wrapped);
}

void MarkdownView::handleLinkClicked(const QUrl &url)
{
    const QString urlStr = url.toString();
    // 1. In-page anchor navigation (e.g. #section or /page#section)
    if (urlStr.startsWith(QLatin1Char('#'))) {
        m_htmlView->scrollToAnchor(urlStr.mid(1));
        return;
    }
    if (url.scheme().isEmpty() && !url.fragment().isEmpty()) {
        m_htmlView->scrollToAnchor(url.fragment());
        return;
    }
    // 2. Regular link activation
    emit linkActivated(url);
}

void MarkdownView::processCompleteLines()
{
    qsizetype start = 0;
    qsizetype newline = 0;
    while ((newline = m_lineBuffer.indexOf(QLatin1Char('\n'), start)) >= 0) {
        const QString line = m_lineBuffer.mid(start, newline - start);
        start = newline + 1;

        const bool beginsBlock = !m_blockHasContent && !line.trimmed().isEmpty();
        m_currentBlock += line;
        m_currentBlock += QLatin1Char('\n');

        if (m_inFence) {
            if (fenceCloser(line)) {
                m_inFence = false;
                commitCurrentBlock();
            } else {
                m_fenceContent += line;
                m_fenceContent += QLatin1Char('\n');
            }
            continue;
        }

        QChar marker;
        int length = 0;
        if (fenceOpener(line, &marker, &length)) {
            m_inFence = true;
            m_fenceMarker = marker;
            m_fenceLength = length;
            m_blockHasContent = true;
            m_fenceContent.clear();
            // md4c treats an EOF-terminated fence as a code block, so it is
            // safe to show the unfinished code block in the live tail.
            m_currentBlockPreviewable = true;
            continue;
        }

        if (line.trimmed().isEmpty()) {
            if (m_blockHasContent)
                commitCurrentBlock();
            else {
                m_currentBlock.clear();
                resetBlockState();
            }
        } else {
            if (beginsBlock)
                m_currentBlockPreviewable = isPreviewableParagraphStart(line);
            m_blockHasContent = true;
            if (containsGfmTable(m_currentBlock))
                m_currentBlockPreviewable = true;
            if (isAtxHeading(line))
                commitCurrentBlock();
        }
    }

    if (start > 0) {
        m_lineBuffer.remove(0, start);
    }
}

void MarkdownView::commitCurrentBlock()
{
    if (m_currentBlock.isEmpty())
        return;

    m_previewTimer.stop();
    const QScrollBar *const verticalBar = m_htmlView->verticalScrollBar();
    const bool followEnd = verticalBar->maximum() == 0
        || verticalBar->value() >= verticalBar->maximum() - kStreamFollowEndTolerance;
    const bool rebuildRenderTree = containsGfmTable(m_currentBlock);
    const QString fragment = m_renderer->renderFragment(m_currentBlock);
    m_renderedHtml += fragment;
    m_pendingPreviewHtml.clear();
    m_htmlView->replaceElementHtml({},
                                   QString::fromLatin1(kPendingStreamElementId),
                                   false,
                                   false);
    const bool appended = m_htmlView->appendHtmlToElement(fragment,
                                                           QString::fromLatin1(kCommittedStreamElementId),
                                                           followEnd,
                                                           false,
                                                           rebuildRenderTree);
    if (!appended) {
        rebuildDocument();
    }
    m_currentBlock.clear();
    resetBlockState();
}

void MarkdownView::previewCurrentBlock()
{
    const QString previewSource = m_currentBlock + m_lineBuffer;
    if (!m_streaming || !m_currentBlockPreviewable || previewSource.isEmpty())
        return;

    const QScrollBar *const verticalBar = m_htmlView->verticalScrollBar();
    const bool followEnd = verticalBar->maximum() == 0
        || verticalBar->value() >= verticalBar->maximum() - kStreamFollowEndTolerance;
    // <pre> and table establish their own formatting context. litehtml's
    // local child replacement does not reliably propagate their changed
    // height to ancestor blocks, so rebuild only for these live-tail types.
    const bool rebuildRenderTree = containsGfmTable(previewSource);
    const bool rebuildRenderSubtree = m_inFence;
    if (m_inFence) {
        // Code is raw text while the fence is open. Do not repeatedly parse
        // incomplete Markdown: preserve whitespace and escape it directly.
        m_pendingPreviewHtml = QStringLiteral("<pre><code>")
            + (m_fenceContent + m_lineBuffer).toHtmlEscaped()
            + QStringLiteral("</code></pre>");
    } else {
        m_pendingPreviewHtml = m_renderer->renderFragment(previewSource);
    }
    m_htmlView->replaceElementHtml(m_pendingPreviewHtml,
                                   QString::fromLatin1(kPendingStreamElementId),
                                   followEnd,
                                   false,
                                   rebuildRenderTree,
                                   rebuildRenderSubtree);
}

void MarkdownView::scheduleCurrentBlockPreview()
{
    if (!m_blockHasContent && m_currentBlock.isEmpty() && !m_lineBuffer.isEmpty())
        m_currentBlockPreviewable = isPreviewableParagraphStart(m_lineBuffer);

    if (m_streaming && m_currentBlockPreviewable
        && (!m_currentBlock.isEmpty() || !m_lineBuffer.isEmpty())
        && !m_previewTimer.isActive()) {
        m_previewTimer.start();
    }
}

void MarkdownView::resetBlockState()
{
    m_blockHasContent = false;
    m_currentBlockPreviewable = false;
    m_inFence = false;
    m_fenceMarker = {};
    m_fenceLength = 0;
    m_fenceContent.clear();
}

void MarkdownView::rebuildDocument()
{
    QString fragment = m_renderedHtml;
    if (m_streaming) {
        fragment = QStringLiteral("<div id=\"") + QString::fromLatin1(kCommittedStreamElementId)
            + QStringLiteral("\" class=\"markdown-stream-content\"><span class=\"markdown-stream-sentinel\">&#8203;</span>") + m_renderedHtml
            + QStringLiteral("</div><div id=\"") + QString::fromLatin1(kPendingStreamElementId)
            + QStringLiteral("\" class=\"markdown-stream-pending\"><span class=\"markdown-stream-sentinel\">&#8203;</span>") + m_pendingPreviewHtml
            + QStringLiteral("</div>");
    }
    m_htmlView->setHtml(m_renderer->wrapDocument(fragment, m_styleSheet.build()));
    m_htmlView->setUrl(m_baseUrl);
}

MarkdownStyleSheet MarkdownView::themeStyleSheet() const
{
    const auto &colors = themeColorsRef();
    MarkdownStyleSheet styleSheet;
    styleSheet.colors = {
        .background = colors.bgLayer,
        .text = colors.textPrimary,
        .secondaryText = colors.textSecondary,
        .border = colors.strokeDefault,
        .link = colors.textAccentPrimary,
        .quoteBackground = colors.bgLayerAlt,
        .codeBackground = colors.bgLayerAlt,
        .tableStripe = colors.bgLayerAlt,
    };
    styleSheet.style.bodyFontFamily = themeFont(Typography::FontRole::Body).toQFont().family();
    return styleSheet;
}

bool MarkdownView::fenceOpener(const QString &line, QChar *marker, int *length)
{
    int position = 0;
    while (position < line.size() && position < 3 && line.at(position) == QLatin1Char(' '))
        ++position;
    if (position >= line.size())
        return false;
    const QChar candidate = line.at(position);
    if (candidate != QLatin1Char('`') && candidate != QLatin1Char('~'))
        return false;
    int count = 0;
    while (position + count < line.size() && line.at(position + count) == candidate)
        ++count;
    if (count < 3)
        return false;
    *marker = candidate;
    *length = count;
    return true;
}

bool MarkdownView::isPreviewableParagraphStart(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('|')))
        return false;

    const QChar first = trimmed.front();
    if (first == QLatin1Char('#') || first == QLatin1Char('`')
        || first == QLatin1Char('~')) {
        return false;
    }

    // Quotes and list items are safe in the live tail: its complete HTML is
    // replaced on each throttled update, then committed on block completion.
    if (first == QLatin1Char('>') || first == QLatin1Char('+'))
        return true;
    if (first == QLatin1Char('-') || first == QLatin1Char('*')) {
        const QString withoutMarker = trimmed.mid(1).trimmed();
        return !withoutMarker.isEmpty() && !withoutMarker.contains(first);
    }

    int index = 0;
    while (index < trimmed.size() && trimmed.at(index).isDigit())
        ++index;
    return index == 0 || index == trimmed.size()
        || (trimmed.at(index) != QLatin1Char('.') && trimmed.at(index) != QLatin1Char(')'))
        || (index + 1 < trimmed.size() && trimmed.at(index + 1).isSpace());
}

bool MarkdownView::containsGfmTable(const QString &markdown)
{
    static const QRegularExpression delimiter(QStringLiteral(
        R"(^\s*\|?\s*:?-{3,}:?\s*(?:\|\s*:?-{3,}:?\s*)+\|?\s*$)"));
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (delimiter.match(line).hasMatch())
            return true;
    }
    return false;
}

bool MarkdownView::isAtxHeading(const QString &line)
{
    const QString trimmed = line.trimmed();
    int index = 0;
    while (index < trimmed.size() && trimmed.at(index) == QLatin1Char('#'))
        ++index;
    return index >= 1 && index <= 6 && index < trimmed.size() && trimmed.at(index).isSpace();
}

bool MarkdownView::fenceCloser(const QString &line) const
{
    int position = 0;
    while (position < line.size() && position < 3 && line.at(position) == QLatin1Char(' '))
        ++position;
    int count = 0;
    while (position + count < line.size() && line.at(position + count) == m_fenceMarker)
        ++count;
    if (count < m_fenceLength)
        return false;
    return line.mid(position + count).trimmed().isEmpty();
}

} // namespace ui::widget
