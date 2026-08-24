#include "MarkdownView.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QMenu>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QScrollBar>
#include <FluentQt/Design.h>

namespace ui::widget {

MarkdownView::MarkdownView(QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_theme(ui::markdown::MarkdownTheme::light(font()))
    , m_resources(this)
{
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    connect(&m_resources, &ui::markdown::MarkdownImageResourceManager::imageUpdated, this, [this] {
        viewport()->update();
    });
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

MarkdownView::~MarkdownView() = default;

void MarkdownView::setMarkdown(const QString &markdown)
{
    if (m_markdown == markdown) return;
    m_markdown = markdown;
    rebuildDocument();
}

QString MarkdownView::markdown() const
{
    return m_markdown;
}

void MarkdownView::clear()
{
    setMarkdown({});
}

void MarkdownView::setHtml(const QString &html)
{
    m_markdown = html;
    rebuildDocument();
}

QString MarkdownView::html() const
{
    return m_markdown;
}

void MarkdownView::beginStream()
{
    m_streaming = true;
    m_markdown.clear();
    m_streamTail.clear();
    m_document = ui::markdown::MarkdownDocument{};
    m_activeTailDocument = ui::markdown::MarkdownDocument{};
    m_stableStreamLayout = {};
    m_stableStreamLayoutDirty = true;
    m_metrics = {};
    relayout();
    emit streamingChanged(true);
    updateAutoFitHeight();
}

void MarkdownView::appendMarkdown(const QString &chunk)
{
    if (chunk.isEmpty()) return;
    m_markdown += chunk;
    m_streamTail += chunk;
    const qsizetype boundary = stableStreamingBoundary();
    if (boundary > 0) {
        QElapsedTimer parseTimer; parseTimer.start();
        m_document.append(m_parser.parse(m_streamTail.left(boundary)));
        m_metrics.lastParseMs = parseTimer.elapsed();
        ++m_metrics.stableParseCount;
        m_streamTail.remove(0, boundary);
        m_stableStreamLayoutDirty = true;
    }
    // Only the active tail is re-parsed per token; stable blocks retain their AST.
    QElapsedTimer tailParseTimer; tailParseTimer.start();
    m_activeTailDocument = m_parser.parse(m_streamTail);
    m_metrics.lastParseMs = tailParseTimer.elapsed();
    ++m_metrics.tailParseCount;
    relayout();
}

void MarkdownView::appendStreamingText(const QString& chunk)
{
    appendMarkdown(chunk);
}

void MarkdownView::appendHtml(const QString &htmlFragment)
{
    appendMarkdown(htmlFragment);
}

void MarkdownView::finishStream()
{
    m_streaming = false;
    emit streamingChanged(false);
    emit streamingFinished();
    m_streamTail.clear();
    m_activeTailDocument = ui::markdown::MarkdownDocument{};
    rebuildDocument();
}

void MarkdownView::finishStreaming()
{
    finishStream();
}

bool MarkdownView::isStreaming() const
{
    return m_streaming;
}

MarkdownViewMetrics MarkdownView::metrics() const noexcept
{
    return m_metrics;
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
    m_theme.bodyFont.setFamily(styleSheet.style.bodyFontFamily);
    if (styleSheet.style.fontSize > 0) m_theme.bodyFont.setPixelSize(styleSheet.style.fontSize);
    m_theme.codeFont.setFamily(styleSheet.style.monospaceFontFamily);
    if (styleSheet.colors.text.isValid()) m_theme.text = styleSheet.colors.text;
    if (styleSheet.colors.link.isValid()) m_theme.link = styleSheet.colors.link;
    if (styleSheet.colors.codeBackground.isValid()) m_theme.codeBackground = styleSheet.colors.codeBackground;
    ++m_theme.version; relayout();
}

MarkdownStyleSheet MarkdownView::markdownStyleSheet() const
{
    return m_styleSheet;
}

void MarkdownView::resetMarkdownStyleSheetToTheme()
{
    m_usesThemeStyleSheet = true;
    onThemeUpdated();
}

void MarkdownView::setTheme(const ui::markdown::MarkdownTheme& theme)
{
    m_theme = theme;
    m_usesThemeStyleSheet = false;
    relayout();
}

ui::markdown::MarkdownTheme MarkdownView::theme() const
{
    return m_theme;
}

void MarkdownView::setBaseFont(const QFont& font)
{
    if (font == m_theme.bodyFont) return;
    m_theme.bodyFont = font;
    m_theme.codeFont.setPointSizeF(font.pointSizeF() * .93);
    ++m_theme.version;
    relayout();
}

void MarkdownView::setContentMargins(const QMarginsF& margins)
{
    if (m_theme.contentMargins == margins) return;
    m_theme.contentMargins = margins;
    ++m_theme.version;
    relayout();
}

void MarkdownView::setTransparentBackground(bool transparent)
{
    m_transparentBackground = transparent;
    viewport()->update();
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
    if (m_usesThemeStyleSheet) {
        const QColor window = palette().color(QPalette::Window);
        m_theme = window.lightness() < 128 ? ui::markdown::MarkdownTheme::dark(font()) : ui::markdown::MarkdownTheme::light(font());
    }
    relayout();
}

void MarkdownView::setAllowNetworkAccess(bool allow)
{
    m_allowNetworkAccess = allow;
    m_resources.setNetworkAccessEnabled(allow);
}

bool MarkdownView::allowNetworkAccess() const
{
    return m_allowNetworkAccess;
}

void MarkdownView::clearResourceCache()
{
    m_resources.clear();
    requestImageResources();
}

void MarkdownView::setImageLoadingEnabled(bool enabled)
{
    setAllowNetworkAccess(enabled);
}

bool MarkdownView::imageLoadingEnabled() const
{
    return allowNetworkAccess();
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
    const QString text = documentPlainText();
    if (!m_selection.isValid()) return {};
    const int begin = qBound(0, qMin(m_selection.anchor, m_selection.position), text.size());
    const int end = qBound(0, qMax(m_selection.anchor, m_selection.position), text.size());
    return text.mid(begin, end - begin);
}

QString MarkdownView::selectedHtml() const
{
    return selectedText().toHtmlEscaped();
}

void MarkdownView::copy()
{
    const QString text = selectedText();
    if (!text.isEmpty()) QGuiApplication::clipboard()->setText(text);
}

void MarkdownView::selectAll()
{
    if (!m_selectable) return;
    m_selection = {0, static_cast<int>(documentPlainText().size())};
    m_selectionAnchor = 0;
    emit selectionChanged(m_selection.isValid());
    viewport()->update();
}

void MarkdownView::setSelectable(bool selectable)
{
    if (m_selectable == selectable) return;
    m_selectable = selectable;
    if (!m_selectable) {
        m_selection = {};
        m_selectionAnchor = -1;
        emit selectionChanged(false);
        viewport()->update();
    }
}

bool MarkdownView::isSelectable() const
{
    return m_selectable;
}

void MarkdownView::setTaskListInteractive(bool interactive)
{
    m_taskListInteractive = interactive;
}

bool MarkdownView::isTaskListInteractive() const
{
    return m_taskListInteractive;
}

void MarkdownView::setZoomFactor(qreal factor)
{
    m_zoomFactor = factor;
    m_theme.bodyFont.setPointSizeF(font().pointSizeF() * qBound<qreal>(0.5, factor, 3.0));
    m_theme.codeFont.setPointSizeF(m_theme.bodyFont.pointSizeF() * .93);
    ++m_theme.version; relayout();
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
    const QString haystack = documentPlainText();
    const Qt::CaseSensitivity caseSensitivity = flags.testFlag(QTextDocument::FindCaseSensitively) ? Qt::CaseSensitive : Qt::CaseInsensitive;
    int from = incremental && m_selection.position >= 0 ? m_selection.position : 0;
    int found = haystack.indexOf(text, from, caseSensitivity);
    bool didWrap = false;
    if (found < 0 && from > 0) { found = haystack.indexOf(text, 0, caseSensitivity); didWrap = found >= 0; }
    if (wrapped) *wrapped = didWrap;
    if (found < 0) return false;
    m_selection = {found, found + static_cast<int>(text.size())}; m_selectionAnchor = found; viewport()->update(); return true;
}

void MarkdownView::updateAutoFitHeight()
{
    const int h = qMax(24, qCeil(m_documentLayout.size.height()));
    if (h != m_autoFitContentHeight) {
        m_autoFitContentHeight = h;
        updateGeometry();
        emit autoFitHeightChanged(h);
    }
}

QSize MarkdownView::sizeHint() const
{
    int h = m_autoFitContentHeight > 0 ? m_autoFitContentHeight : 34;
    if (h <= 0) h = 34;
    return QSize(256, h);
}

QSize MarkdownView::minimumSizeHint() const
{
    return QSize(50, sizeHint().height());
}

void MarkdownView::wheelEvent(QWheelEvent *event)
{
    QAbstractScrollArea::wheelEvent(event);
}

void MarkdownView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    relayout();
}

void MarkdownView::rebuildDocument()
{
    QElapsedTimer parseTimer; parseTimer.start();
    m_document = m_parser.parse(m_markdown);
    m_metrics.lastParseMs = parseTimer.elapsed();
    ++m_metrics.fullParseCount;
    m_activeTailDocument = ui::markdown::MarkdownDocument{};
    m_stableStreamLayout = {};
    m_stableStreamLayoutDirty = true;
    relayout();
}

void MarkdownView::relayout()
{
    const qreal contentWidth = qMax(1, viewport()->width());
    if (!m_streaming) {
        m_documentLayout = m_layoutEngine.layout(m_document, contentWidth, m_theme);
    } else {
        if (m_stableStreamLayoutDirty || m_stableStreamLayoutWidth != contentWidth || m_stableStreamThemeVersion != m_theme.version) {
            QElapsedTimer stableTimer; stableTimer.start();
            m_stableStreamLayout = m_layoutEngine.layout(m_document, contentWidth, m_theme);
            m_metrics.lastStableLayoutMs = stableTimer.elapsed();
            ++m_metrics.stableLayoutCount;
            m_stableStreamLayoutDirty = false;
            m_stableStreamLayoutWidth = contentWidth;
            m_stableStreamThemeVersion = m_theme.version;
        }
        QElapsedTimer tailTimer; tailTimer.start();
        const ui::markdown::DocumentLayout tail = m_layoutEngine.layout(m_activeTailDocument, contentWidth, m_theme);
        m_metrics.lastTailLayoutMs = tailTimer.elapsed();
        ++m_metrics.tailLayoutCount;
        m_documentLayout = m_stableStreamLayout;
        const qreal yOffset = m_stableStreamLayout.size.height() - m_theme.contentMargins.bottom() - m_theme.contentMargins.top();
        const int textOffset = m_stableStreamLayout.textLength();
        for (ui::markdown::BlockLayout block : tail.blocks) {
            block.rect.translate(0, yOffset);
            block.copyButtonRect.translate(0, yOffset);
            block.documentTextOffset += textOffset;
            m_documentLayout.blocks.push_back(std::move(block));
        }
        m_documentLayout.size = QSizeF(contentWidth, yOffset + tail.size.height());
        m_documentLayout.width = contentWidth;
        m_documentLayout.themeVersion = m_theme.version;
    }
    updateScrollBars(); updateAutoFitHeight(); viewport()->update();
    m_metrics.blockCount = m_documentLayout.blocks.size();
    m_metrics.documentHeight = m_documentLayout.size.height();
    requestImageResources();
    if (m_lastDocumentSize != m_documentLayout.size) {
        m_lastDocumentSize = m_documentLayout.size;
        emit documentSizeChanged(m_lastDocumentSize);
    }
}

void MarkdownView::requestImageResources()
{
    for (const ui::markdown::BlockLayout& block : m_documentLayout.blocks) {
        if (block.kind == ui::markdown::BlockKind::Image)
            m_resources.request(block.imageUrl, m_baseUrl);
    }
}

void MarkdownView::updateScrollBars()
{
    const int height = qCeil(m_documentLayout.size.height());
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0, qMax(0, height - viewport()->height()));
    setVerticalScrollBarPolicy(m_autoFitHeight ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

QPointF MarkdownView::toDocument(const QPointF& viewportPosition) const { return viewportPosition + QPointF(0, verticalScrollBar()->value()); }

QString MarkdownView::documentPlainText() const
{
    QString text;
    for (const auto& block : m_documentLayout.blocks) {
        if (block.inlineLayout) text += block.inlineLayout->text;
        else if (block.kind == ui::markdown::BlockKind::CodeBlock) text += block.code;
        text += u'\n';
    }
    return text;
}

void MarkdownView::setSelectionPosition(int position, bool extend)
{
    if (position < 0) return;
    if (!extend || m_selectionAnchor < 0) m_selectionAnchor = position;
    m_selection = {m_selectionAnchor, position};
    emit selectionChanged(m_selection.isValid());
    viewport()->update();
}

void MarkdownView::showCopiedFeedback(int blockIndex)
{
    m_copiedBlock = blockIndex;
    viewport()->update();
    QTimer::singleShot(1200, this, [this, blockIndex] {
        if (m_copiedBlock != blockIndex) return;
        m_copiedBlock = -1;
        viewport()->update();
    });
}

void MarkdownView::toggleTask(const ui::markdown::BlockLayout& block)
{
    if (block.taskSourceLine <= 0) return;
    int start = 0;
    for (int line = 1; line < block.taskSourceLine; ++line) {
        start = m_markdown.indexOf(u'\n', start);
        if (start < 0) return;
        ++start;
    }
    const int end = m_markdown.indexOf(u'\n', start);
    const int length = (end < 0 ? m_markdown.size() : end) - start;
    const QString line = m_markdown.mid(start, length);
    const QRegularExpression checkbox(QStringLiteral("\\[([ xX])\\]"));
    const QRegularExpressionMatch match = checkbox.match(line);
    if (!match.hasMatch()) return;
    const bool checked = !block.taskChecked;
    m_markdown.replace(start + match.capturedStart(1), 1, checked ? QStringLiteral("x") : QStringLiteral(" "));
    emit taskToggled(block.taskSourceLine, checked);
    rebuildDocument();
}

qsizetype MarkdownView::stableStreamingBoundary() const
{
    bool inFence = false;
    qsizetype lastBoundary = 0;
    qsizetype start = 0;
    while (start <= m_streamTail.size()) {
        const qsizetype end = m_streamTail.indexOf(u'\n', start);
        const qsizetype lineEnd = end < 0 ? m_streamTail.size() : end;
        const QStringView line{m_streamTail.constData() + start, lineEnd - start};
        const QStringView trimmed = line.trimmed();
        if (trimmed.startsWith(u"```") || trimmed.startsWith(u"~~~")) inFence = !inFence;
        if (!inFence && trimmed.isEmpty()) lastBoundary = end < 0 ? lineEnd : end + 1;
        if (end < 0) break;
        start = end + 1;
    }
    return lastBoundary;
}

void MarkdownView::paintViewport(QPaintEvent* event)
{
    QElapsedTimer paintTimer; paintTimer.start();
    QPainter painter(viewport());
    if (!m_transparentBackground) painter.fillRect(event->rect(), m_theme.background.isValid() ? m_theme.background : palette().color(QPalette::Base));
    painter.save();
    painter.translate(0, -verticalScrollBar()->value());
    m_metrics.visibleBlockCount = m_renderer.paint(painter, m_documentLayout, m_theme, QRectF(event->rect()).translated(0, verticalScrollBar()->value()), m_selection, m_hoveredBlock, m_resources.images(), m_copiedBlock);
    painter.restore();
    if (hasFocus()) {
        painter.setPen(QPen(m_theme.link, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(viewport()->rect().adjusted(0, 0, -1, -1));
    }
    m_metrics.lastPaintMs = paintTimer.elapsed();
}

bool MarkdownView::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::Paint) { paintViewport(static_cast<QPaintEvent*>(event)); return true; }
    if (event->type() == QEvent::MouseMove) {
        auto* mouse = static_cast<QMouseEvent*>(event); const auto hit = m_renderer.hitTest(m_documentLayout, toDocument(mouse->position()));
        m_hoveredBlock = hit.blockIndex; viewport()->setCursor(hit.kind == ui::markdown::HitKind::Link || hit.kind == ui::markdown::HitKind::CodeCopy ? Qt::PointingHandCursor : Qt::IBeamCursor);
        if (m_selecting) setSelectionPosition(hit.textOffset, true);
        if (hit.kind == ui::markdown::HitKind::Link) emit linkHighlighted(QUrl(hit.value)); viewport()->update(); return true;
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event); if (mouse->button() == Qt::LeftButton) { const auto hit = m_renderer.hitTest(m_documentLayout, toDocument(mouse->position())); m_selecting = m_selectable && (hit.kind == ui::markdown::HitKind::Text || hit.kind == ui::markdown::HitKind::Link); if (m_selecting) setSelectionPosition(hit.textOffset, mouse->modifiers().testFlag(Qt::ShiftModifier)); return true; }
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event); if (mouse->button() == Qt::LeftButton) { const auto hit = m_renderer.hitTest(m_documentLayout, toDocument(mouse->position())); if (m_selecting && m_selection.anchor == m_selection.position && hit.kind == ui::markdown::HitKind::Link) emit linkActivated(QUrl(hit.value)); if (hit.kind == ui::markdown::HitKind::Image) emit imageActivated(QUrl(hit.value)); if (hit.kind == ui::markdown::HitKind::CodeCopy) { QGuiApplication::clipboard()->setText(hit.value); showCopiedFeedback(hit.blockIndex); } if (hit.kind == ui::markdown::HitKind::TaskCheckbox && m_taskListInteractive && hit.blockIndex >= 0) toggleTask(m_documentLayout.blocks.at(hit.blockIndex)); m_selecting = false; return true; }
    }
    if (event->type() == QEvent::ContextMenu) {
        auto* context = static_cast<QContextMenuEvent*>(event);
        const auto hit = m_renderer.hitTest(m_documentLayout, toDocument(context->pos()));
        emit contextMenuRequested(context->pos(), hit.kind == ui::markdown::HitKind::Link ? QUrl(hit.value) : QUrl{}, hit.kind == ui::markdown::HitKind::Image ? QUrl(hit.value) : QUrl{});
        QMenu menu(this);
        if (hit.kind == ui::markdown::HitKind::Link) {
            QAction* open = menu.addAction(tr("打开链接"));
            connect(open, &QAction::triggered, this, [this, url = QUrl(hit.value)] { emit linkActivated(url); });
            QAction* copyLink = menu.addAction(tr("复制链接"));
            connect(copyLink, &QAction::triggered, this, [url = hit.value] { QGuiApplication::clipboard()->setText(url); });
        } else if (hit.kind == ui::markdown::HitKind::CodeCopy || (hit.blockIndex >= 0 && m_documentLayout.blocks.at(hit.blockIndex).kind == ui::markdown::BlockKind::CodeBlock)) {
            const QString code = hit.kind == ui::markdown::HitKind::CodeCopy ? hit.value : m_documentLayout.blocks.at(hit.blockIndex).code;
            QAction* copyCode = menu.addAction(tr("复制代码"));
            connect(copyCode, &QAction::triggered, this, [this, code, index = hit.blockIndex] { QGuiApplication::clipboard()->setText(code); showCopiedFeedback(index); });
        } else {
            QAction* copyText = menu.addAction(tr("复制"));
            copyText->setEnabled(m_selection.isValid());
            connect(copyText, &QAction::triggered, this, &MarkdownView::copy);
            QAction* selectAll = menu.addAction(tr("全选"));
            connect(selectAll, &QAction::triggered, this, [this] { m_selection = {0, static_cast<int>(documentPlainText().size())}; m_selectionAnchor = 0; emit selectionChanged(true); viewport()->update(); });
        }
        menu.exec(context->globalPos());
        return true;
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void MarkdownView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::SelectAll)) { selectAll(); event->accept(); return; }
    if (event->matches(QKeySequence::Copy)) { copy(); event->accept(); return; }
    QAbstractScrollArea::keyPressEvent(event);
}

} // namespace ui::widget
